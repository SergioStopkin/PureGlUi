# common/ - Common:: primitives

The `Common::` layer is a set of cross-cutting primitives that depend on nothing (only the C++ standard library and, in one case, the OS). The `ui/` layer depends on `common/`, never the reverse.

## Common::Bit

Header: `include/common/bit.h`

Static-only utility class (all constructors/destructor deleted) providing type-safe bitwise operations. Each operation widens its operands through their unsigned counterparts before combining, so signed enum/integer types cannot corrupt the high bits or trigger signed-overflow UB. All methods are `static constexpr`.

Signatures:

```cpp
template <typename T1, typename T2> static constexpr auto Or(T1 a, T2 b);
template <typename T1, typename T2> static constexpr auto And(T1 a, T2 b);
template <typename T1, typename T2> static constexpr auto Xor(T1 a, T2 b);
template <typename T> static constexpr auto Shl(T val, unsigned int shift);
template <typename T> static constexpr auto Shr(T val, unsigned int shift);
```

- `Or` / `And` / `Xor` - bitwise OR / AND / XOR of `a` and `b`. Operands are cast to `std::make_unsigned_t` of their type, then to their `std::common_type_t`; the return type is that common unsigned type.
- `Shl` / `Shr` - left / right shift of `val` by `shift`. `val` is cast to `std::make_unsigned_t<T>`; the return type is that unsigned type.

Notes: used to combine `Changed`-style bitflags without signed-shift warnings. Return type is deduced (`auto`); assign to `auto` or cast at the call site if you need a specific enum type back.

```cpp
const auto merged = Common::Bit::Or(flagsA, flagsB);
const auto masked = Common::Bit::And(state, mask);
```

## Common::loadJson

Header: `include/common/json.h`

Free function - domain-blind JSON file parse shared by the UI resource loaders and any host config loaders.

```cpp
inline bool loadJson(const std::string & file, nlohmann::json & j);
```

- `file` - path to the JSON file to read.
- `j` - out-parameter; the parsed document is written here on success.
- Returns `true` on success, `false` on open or parse failure.

Notes: opens `file` with an `std::ifstream`; on open failure or any parse exception it logs to `std::cerr` (`Cannot open file [...]` or `JSON parse error in file [...]`) and returns `false`. Never throws - all parse exceptions are caught internally. Depends on `nlohmann/json.hpp`.

```cpp
nlohmann::json doc;
if (!Common::loadJson(path, doc)) {
    return; // already logged
}
```

## Common::Sanitize

Header: `include/common/sanitize.h`

Static-only validator class (all constructors/destructor deleted) for strings ingested from user-editable / res JSON. res/ is a trust boundary, so every string field is sanitized at parse time; each validator logs a warning to `std::cerr` when it modifies or rejects a value.

Length constants (`static constexpr std::size_t`):

- `MAX_STRING_LENGTH = 4096` - cap for locale/UI strings.
- `MAX_URL_LENGTH = 2048` - cap for URL strings.
- `MAX_FILE_SIZE = 65536` - cap for file content (64 KB).
- `MAX_PATH_LENGTH = 4096` - cap for an absolute filesystem path (matches POSIX PATH_MAX on Linux).

Methods (all `static`):

```cpp
static std::string string(std::string_view input, std::string_view key,
                          std::size_t maxLength = MAX_STRING_LENGTH);
static std::string filePath(std::string_view input, std::string_view key);
static std::string path(std::string_view input, std::string_view key);
static std::string fileContent(std::string_view input, std::string_view path);
static std::string url(std::string_view input, std::string_view key);
```

- `string` - strips control characters (keeps `\n` and `\t`; drops everything below 0x20 and the 0x7F DEL byte, comparing through `unsigned char` so UTF-8 continuation bytes survive), truncates to `maxLength`. `key` is only used to identify the field in log messages. Returns the sanitized string.
- `filePath` - a plain filename within the resource directory. Runs `string` with a 255-char cap, then rejects (returns empty) absolute paths (leading `/` or `\`) and any value containing `..`. For bundled res filenames, not OS paths.
- `path` - an absolute OS path from user-editable JSON (session paths, last-open-dir). Policy differs from `filePath`: absolute paths and `..` are allowed, but an embedded NUL aborts the value (returns empty), and all C0 control chars including `\n`/`\t` are stripped. Capped at `MAX_PATH_LENGTH`. Returns empty on empty input or embedded NUL.
- `fileContent` - convenience wrapper: `string(input, path, MAX_FILE_SIZE)`. The second argument names the file for logging.
- `url` - runs `string` with `MAX_URL_LENGTH`, then requires the result to start with `https://`; otherwise logs and returns empty.

Notes: use `filePath` for bundled resource path fields, `path` for real OS paths, `url` for real URLs, and `string` for everything else (labels, keys, titles, content). Audit rule: any `.value("` in a loader must be wrapped in the type-appropriate call (bool/number fields excepted).

```cpp
const std::string label = Common::Sanitize::string(node.value("label", ""), "label");
const std::string icon  = Common::Sanitize::filePath(node.value("icon", ""), "icon");
const std::string dir   = Common::Sanitize::path(node.value("lastOpenDir", ""), "lastOpenDir");
```

## Common::Unicode

Header: `include/common/unicode.h`

Static-only conversion class (all constructors/destructor and assignment deleted). Single source of Unicode conversions. The canonical in-memory text type is `std::wstring`, which carries UTF-16 (with surrogate pairs) where `sizeof(wchar_t) == 2` (Windows) and UTF-32 codepoints where it is 4 bytes (Linux, macOS); all conversions handle surrogate encode/decode transparently. Invalid input decodes to U+FFFD (the replacement character).

Methods (all `static`):

```cpp
static std::wstring   fromUtf8 (std::string_view    text);
static std::string    toUtf8   (std::wstring_view   text);
static std::wstring   fromUtf16(std::u16string_view text);
static std::u16string toUtf16  (std::wstring_view   text);
static std::wstring   fromUtf32(std::u32string_view text);
static std::u32string toUtf32  (std::wstring_view   text);
```

- `fromUtf8` / `toUtf8` - convert between UTF-8 `std::string` and the canonical `std::wstring`.
- `fromUtf16` / `toUtf16` - convert between UTF-16 `std::u16string` and `std::wstring`.
- `fromUtf32` / `toUtf32` - convert between UTF-32 `std::u32string` and `std::wstring`.

Notes: `fromUtf8`/`toUtf8` are the pair used across the text-rendering pipeline. Malformed byte sequences, unexpected continuation bytes, or truncated sequences yield U+FFFD rather than throwing.

```cpp
const std::wstring wide = Common::Unicode::fromUtf8(utf8Label);
const std::string  back = Common::Unicode::toUtf8(wide);
```

## Common::dirExists

Header: `include/common/fs.h`

Free function.

```cpp
inline bool dirExists(const std::string & dir);
```

- `dir` - path to check.
- Returns `true` if `dir` exists and is a directory; otherwise logs `Directory does not exist or is not a directory: ...` to `std::cerr` and returns `false`.

Notes: intended to guard `std::filesystem::directory_iterator` loops, which throw on a missing path (unlike the file loaders, which fail gracefully through `Common::loadJson`).

```cpp
if (!Common::dirExists(themeDir)) {
    return;
}
for (const auto & entry : std::filesystem::directory_iterator(themeDir)) {
    // ...
}
```

## Common::System

Header: `include/common/system.h`

Static-only host/system-info class (all constructors/destructor deleted). OS primitive; depends only on the platform plus `Common::Unicode`. Platform branches cover Linux, macOS (APPLE), and Windows; the header also defines the `WINDOWS`/`APPLE`/`LINUX` platform macros and (on Windows) `NOMINMAX`.

Methods (all `static`):

```cpp
static unsigned int cpuCores();
static std::string  cpuFrequency();
static std::string  systemRam();
static void         setSystemName(const std::string & name);
```

- `cpuCores` - number of hardware threads via `std::thread::hardware_concurrency()`, clamped to a minimum of 1.
- `cpuFrequency` - base CPU clock formatted as `"X.XX GHz"`, or `"N/A"` if unavailable. Linux prefers the rated frequency in `/proc/cpuinfo` `model name` (`@ ...GHz`), falling back to sysfs `cpuinfo_max_freq`; macOS uses `hw.cpufrequency_max` and falls back to parsing the CPU brand string (Apple Silicon exposes no frequency sysctl); Windows reads `~MHz` from the CPU registry key.
- `systemRam` - total physical RAM as `"N GB"`, or `"N/A"` if unavailable. Linux reads `/proc/meminfo` `MemTotal`; macOS reads `hw.memsize`; Windows uses `GlobalMemoryStatusEx` and rounds to the nearest GB.
- `setSystemName` - sets the application name shown by the OS (Windows AppUserModelID for taskbar grouping; macOS NSApplication dock name). No-op where unsupported; the `name` argument is ignored on those platforms.

Notes: the frequency/RAM strings are display-oriented (used to resolve dialog placeholders such as %CPU%/%RAM%), not machine-readable numbers.

```cpp
const unsigned int cores = Common::System::cpuCores();
const std::string  ram   = Common::System::systemRam();   // e.g. "16 GB"
Common::System::setSystemName("PureGlUi");
```

## Common::BackgroundWorker / Common::backgroundWorker

Header: `include/common/backgroundworker.h`

A fixed-size background task pool with per-task CPU budgeting and safe shutdown. Moves heavy work (resource loads, file I/O, teardown, session writes) off the UI thread. Inherits privately from `Common::NonCopyable`.

Construction: the pool spins up `max(1, cpuCores - 2)` worker threads (two cores reserved for the UI/render thread and the OS) draining a FIFO queue.

Public API:

```cpp
BackgroundWorker();
~BackgroundWorker();
void post(unsigned int threads, std::function<void()> task);
void post(std::function<void()> task);           // single-core convenience
[[nodiscard]] unsigned int budget() const;
```

- `post(threads, task)` - enqueue `task` declaring how many cores it intends to use. Returns immediately; the task runs once that many cores of the pool budget are free. `threads == 0` is treated as 1; a request above the budget is clamped to the budget, so a task can never deadlock by asking for more cores than exist. Admission is FIFO - a large task at the head holds back cheaper tasks behind it until enough cores free up.
- `post(task)` - single-core overload (the common case) for file I/O, session writes, teardown.
- `budget()` - total cores the pool will hand out across all in-flight tasks.

Destruction: signals stop, drains tasks already queued (they are guaranteed to run), then joins - workers never outlive the process. Exit blocks briefly if a slow task is still running. Tasks posted after stop (only during shutdown) are silently dropped.

Process-scoped accessor:

```cpp
inline BackgroundWorker & backgroundWorker();
```

Lazy-initialized, one instance per process (C++11-safe). Its static destructs after `main()` returns and after all stack/member objects are gone, so code calling `backgroundWorker().post()` during teardown still gets a live worker.

Gotcha: `std::function` requires a copy-constructible callable, so move-only captures (`std::unique_ptr`, `std::thread`, etc.) must be wrapped in a `std::shared_ptr` by the caller before capture.

```cpp
Common::backgroundWorker().post([path]() {
    // runs on a worker thread; single core
    writeSessionFile(path);
});

Common::backgroundWorker().post(4, [work = std::move(sharedWork)]() {
    // admitted only when 4 cores of the budget are free
    runParallel(work);
});
```

## Common::NonCopyable

Header: `include/common/noncopyable.h`

Mixin base for non-value types (those holding a reference/const member or owning a resource). Deletes copy and move construction/assignment in one place instead of four `= delete` lines per class. Inherit privately.

```cpp
class NonCopyable {
protected:
    NonCopyable()  = default;
    ~NonCopyable() = default;
public:
    NonCopyable(const NonCopyable &)             = delete;
    NonCopyable(NonCopyable &&)                  = delete;
    NonCopyable & operator=(const NonCopyable &) = delete;
    NonCopyable & operator=(NonCopyable &&)      = delete;
};
```

Notes: the destructor is protected and non-virtual (Core Guideline C.35) - the type is only ever a private base, never deleted polymorphically, so it carries no vtable and empty-base optimization keeps it zero-size.

```cpp
class MyResource final : private Common::NonCopyable {
    // copy and move are deleted via the base
};
```

## See also

[vocabulary.md](vocabulary.md), [README.md](README.md)
