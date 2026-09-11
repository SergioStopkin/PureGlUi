# ui/ vocabulary - Ui:: value types

The shared, domain-blind value types the whole framework speaks. Everything here lives under `include/ui/` and depends only on `common/` (the dependency arrow points `app -> ui -> common`); no type here knows anything about a host domain.

## Ui:: primitive aliases

Header: `include/ui/type.h`

Framework-owned primitive aliases, so `ui/` never has to reach into a host's types.

| Name | Definition | Meaning |
| --- | --- | --- |
| `Ui::id_t` | `std::size_t` | Numeric element/collection identity. |
| `Ui::key_t` | `std::string` | Hierarchical element identity, e.g. `"view:displayMode:shaded"`; `""` = none. |
| `Ui::fpx_t` | `float` | Framework float type (pixels and any other float field). |
| `Ui::surface_id_t` | `std::size_t` | Content-surface identity. |
| `Ui::font_handle_t` | `std::size_t` | Opaque font handle. |
| `Ui::image_handle_t` | `std::size_t` | Opaque image handle. |
| `Ui::task_fn_t` | `std::function<void()>` | Argless callback. |
| `Ui::provider_fn_t` | `std::function<std::string()>` | Produces a current string value. |
| `Ui::action_fn_t` | `std::function<void(const std::string & arg)>` | Consumes an opaque arg (action handler). |
| `Ui::predicate_fn_t` | `std::function<bool(const std::string & arg)>` | Tests a string. |

Constant:

```cpp
inline constexpr Ui::id_t Ui::INVALID_ID = static_cast<id_t>(-1);
```

Sentinel for "no id". Default value of `intent_t::id` and `tab_t::id`.

## Ui::PI

Header: `include/ui/const.h`

```cpp
inline constexpr Ui::fpx_t Ui::PI = std::numbers::pi_v<fpx_t>;
```

Pi as `fpx_t`, shared by degree/radian conversions so `std::numbers::pi_v` is not repeated at call sites.

## Ui::Codepoint + Ui::wstr()

Header: `include/ui/codepoint.h`

Named Unicode codepoints used as UI chrome (ellipsis, leader dots, etc.). Underlying type is `char32_t` (portable for astral codepoints; `wchar_t` is 16-bit on Windows).

```cpp
enum class Ui::Codepoint : char32_t {
    TwoDotH   = 0x2025, // two dot (horizontal)
    ThreeDotH = 0x2026, // three dot (horizontal)
};

[[nodiscard]] inline std::wstring Ui::wstr(Codepoint cp);
```

`wstr(cp)` builds a `std::wstring` carrying the single codepoint. Delegates to `Common::Unicode::fromUtf32`, so the Windows 16-bit `wchar_t` case is encoded as a surrogate pair; on Linux/macOS it is a single wide char.

```cpp
const std::wstring ellipsis = Ui::wstr(Ui::Codepoint::ThreeDotH);
```

## Ui::Color

Header: `include/ui/color.h`

Unified RGBA-byte color (`uint8_t` per channel, 0-255) for all rendering systems, with conversions to OpenGL floats, hex strings and ARGB32. Default-constructed value is opaque magenta (a debug sentinel). Carries an `inherit` flag used by the theme cascade.

Constructors and factories:

```cpp
Color();                                             // opaque magenta (debug default)
Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
Color(int r, int g, int b, int a = 255);
static Color Inherit();                              // marks "inherit from parent"
static Color TransparentBlack();                     // {0,0,0,0}
static Color fromHex(const std::string & hex);       // #RGB, #RRGGBB, #RRGGBBAA; invalid -> default magenta
```

Queries and getters:

```cpp
[[nodiscard]] bool isInherit() const;
[[nodiscard]] uint8_t r() const;
[[nodiscard]] uint8_t g() const;
[[nodiscard]] uint8_t b() const;
[[nodiscard]] uint8_t a() const;
bool operator==(const Color &) const;
bool operator!=(const Color &) const;
```

Conversions:

```cpp
[[nodiscard]] std::array<float, 3> toGLRGB() const;   // 0.0-1.0
[[nodiscard]] std::array<float, 4> toGLRGBA() const;  // 0.0-1.0
[[nodiscard]] uint32_t toArgb32() const;              // 0xAARRGGBB, non-premultiplied
[[nodiscard]] std::string toHex(bool includeAlpha = false) const;
```

Lightness math (operates in HSL; percentage points in `[0,100]`):

```cpp
[[nodiscard]] Color Lighter(float p) const;                       // +p L
[[nodiscard]] Color Darker(float p) const;                        // -p L
[[nodiscard]] Color edgeColor(float contrast, bool isDarkTheme) const; // Lighter on dark, Darker on light
```

`fromHex` accepts short `#RGB` (each digit doubled), `#RRGGBB` and `#RRGGBBAA`; anything else returns the default. HSL conversion is internal.

```cpp
const Ui::Color accent  = Ui::Color::fromHex("#3d7eff");
const auto      gl      = accent.toGLRGBA();
const Ui::Color hovered = accent.Lighter(8.0F);
```

## Ui::config_t + Ui::g_config + scale math

Header: `include/ui/config.h`

`config_t` is the display config the host hands the framework so rendering reads scale/dpi from here rather than a process global. It owns the CSS/physical pixel math.

```cpp
struct alignas(16) Ui::config_t final {
    int   dpi           = 96;    // physical dots per inch
    fpx_t scale         = 1.0F;  // physical px per CSS px (dpi / 96)
    bool  isCompositing = false; // UI composited into one surface (XWayland workaround)

    // CSS px -> physical px
    [[nodiscard]] fpx_t toPhys(int css) const;        // raw scale
    [[nodiscard]] fpx_t toPhys(fpx_t css) const;
    [[nodiscard]] int   toPhysFloor(int css) const;   // floor to int (font/texture sizes)
    [[nodiscard]] int   toPhysFloor(fpx_t css) const;
    [[nodiscard]] fpx_t toPhysRound(int css) const;
    [[nodiscard]] fpx_t toPhysRound(fpx_t css) const; // round CSS to int before scaling (popup pos/size)

    // physical px -> CSS px
    [[nodiscard]] fpx_t toCss(int phys) const;
    [[nodiscard]] fpx_t toCss(fpx_t phys) const;
    [[nodiscard]] int   toCssFloor(int phys) const;
    [[nodiscard]] int   toCssFloor(fpx_t phys) const;
};
```

Canonical instance the host updates:

```cpp
inline Ui::config_t Ui::g_config;
```

Free-function shims forward to `g_config` so call sites spell the math plainly:

```cpp
inline int   Ui::roundToInt(fpx_t v);   // std::lround -> int
inline int   Ui::roundToInt(double v);
inline fpx_t Ui::toPhys(int css);
inline fpx_t Ui::toPhys(fpx_t css);
inline int   Ui::toPhysFloor(int css);
inline int   Ui::toPhysFloor(fpx_t css);
inline fpx_t Ui::toPhysRound(int css);
inline fpx_t Ui::toPhysRound(fpx_t css);
inline fpx_t Ui::toCss(int phys);
inline fpx_t Ui::toCss(fpx_t phys);
inline int   Ui::toCssFloor(int phys);
inline int   Ui::toCssFloor(fpx_t phys);
```

Notes: `toPhys` applies raw `scale`; `toPhysFloor` truncates for integer sizes; `toPhysRound` snaps the CSS value to the pixel grid before scaling. The `int`-overload `toPhysRound(int)` is identical to `toPhys(int)` (no fractional CSS input to round).

```cpp
Ui::g_config.scale = 1.5F;                 // host sets scale for a HiDPI display
const int physW = Ui::toPhysFloor(200);    // 300 physical px for a 200 CSS-px region
const fpx_t css = Ui::toCss(physW);        // back to CSS px
```

## Ui::Convert

Header: `include/ui/convert.h`

Static-only utility (deleted ctor/dtor/copy) that parses CSS-like strings. Every method is failure-safe: on a parse error it returns 0 / `0.0F` / `{}` rather than throwing.

```cpp
static int           Ui::Convert::str2int(const std::string & val, int base = 0, int ref = 0);
static fpx_t         Ui::Convert::str2fpx(const std::string & val, int base = 0, int ref = 0);
static std::uint32_t Ui::Convert::str2uint32(const std::string & val, int base = 0, std::uint32_t ref = 0);
static float         Ui::Convert::parseCssNumber(const std::string & css);
static int           Ui::Convert::parseCssInt(const std::string & css);
static Res::Type::border_t Ui::Convert::parseCssBorderRadius(const std::string & css);
```

- `str2int` strips whitespace and handles `px`, `%`, `vw`, `vh`, and plain integers. For `%`/`vw`/`vh`, if `ref > 0` the result is a percentage of `ref`, otherwise the bare number is returned.
- `str2fpx` delegates to `str2int` and casts to `fpx_t` (unit-aware, integer-valued).
- `str2uint32` is the unsigned peer of `str2int` (same unit handling).
- `parseCssNumber` parses a unitless float (`std::stof`) - use it for opacity, line-height, raw counts; it does NOT strip `px`/`%`/`vw`/`vh`.
- `parseCssInt` parses a leading integer (`std::stoi`), e.g. `"14px" -> 14`.
- `parseCssBorderRadius` parses the CSS `border-radius` shorthand (1/2/3/4 space-separated values) into a `Ui::Res::Type::border_t` (top-left, top-right, bottom-right, bottom-left).

```cpp
const int   w = Ui::Convert::str2int("50%", 0, 800);   // 400
const float o = Ui::Convert::parseCssNumber("0.15");   // 0.15F
const auto  r = Ui::Convert::parseCssBorderRadius("8 4"); // TL/BR=8, TR/BL=4
```

## Ui::Registry<Key, T>

Header: `include/ui/registry.h`

Ordered, id-keyed container: an `unordered_map<id_t, T>` for O(1) identity lookup plus a `vector<id_t>` holding visual/iteration order. The caller always supplies the id on add - the container never invents identity. "Active" state is intentionally NOT modelled here; it lives on the owner.

```cpp
template <typename Key, typename T>
class Ui::Registry final {
public:
    // read side
    [[nodiscard]] const T * find(const Key & key) const;         // nullptr if absent
    [[nodiscard]] bool contains(const Key & key) const;
    [[nodiscard]] const std::vector<Key> & order() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;

    // mutate side
    void add(const Key & key, T value);                          // no-op if key exists
    void insert(std::size_t index, const Key & key, T value);    // clamps index
    void remove(const Key & key);
    void move(const Key & key, std::size_t index);               // reorder; clamps index
    [[nodiscard]] T * edit(const Key & key);                     // mutable access, nullptr if absent
    [[nodiscard]] T & findOrAdd(const Key & key);                // existing, or default-constructed at the end
};
```

The key is a template parameter, so a registry is not limited to `id_t`. `add`/`insert` are no-ops if the key already exists. `insert`/`move` clamp an out-of-range index to the end.

```cpp
Ui::Registry<Ui::id_t, Ui::tab_t> tabs;
tabs.add(1, myTab);
for (const Ui::id_t id : tabs.order()) {
    const Ui::tab_t * tab = tabs.find(id);
    // render tab...
}
```

## Ui::Index<Key, T>

Header: `include/ui/index.h`

Group-by: one key to MANY values, for the acceleration structures that would otherwise be a scan inside a loop. Built once from a flat collection, then read O(1) per key - a parent-to-children index over a tree being the case it exists for.

```cpp
template <typename Key, typename T>
class Ui::Index final {
public:
    void add(const Key & key, T value);
    [[nodiscard]] const std::vector<T> & group(const Key & key) const;  // empty group if absent, never null
    [[nodiscard]] bool contains(const Key & key) const;
    [[nodiscard]] std::size_t size() const;                             // distinct KEYS, not values
    [[nodiscard]] bool empty() const;
    [[nodiscard]] const std::vector<Key> & keys() const;                // first-seen order
};
```

Composed over `Registry` rather than reimplementing map + order, so the two stay one storage strategy: `Registry` means "one value per key, in order", `Index` means "many values per key". Keeping them separate keeps `Registry`'s meaning intact. Insertion order is preserved within a group, which is what a tree walk needs - children come out in the order the source listed them.

## Ui::ElementId

Header: `include/ui/elementid.h`

Element-id range starts the resource loader assigns from at load time. Each authored element gets `base + counter` so a click - which carries only a numeric id - routes back to its `actionKey`.

```cpp
enum class Ui::ElementId : id_t {
    MenuBase       = 1000,    // 1000-1999: menu buttons
    ButtonBase     = 2000,    // 2000-2999: toolbar buttons
    ItemBase       = 5000,    // 5000-5999: menu items
    DockGripBase   = 90'000,  // + dock id
    DockScrollBase = 95'000,  // + dock id
    DockRowBase    = 100'000, // + HOST row id, open-ended
};
```

`9997-9999` are the framework's single-element ids, declared beside the enum: `TAB_ARROW_LEFT = 9997`, `TAB_ARROW_RIGHT = 9998`, `STATUS_TEXT_ID = 9999`.

`DockRowBase` is open-ended and sits far above the rest because dock content is host-projected: the host supplies the row id and the framework only offsets it, applying the base when a row becomes an element and removing it when the intent is built, so a host id round-trips unchanged. The ranges never overlap because the id space is global: one id names one element across the whole system.

## Ui::tab_t

Header: `include/ui/tab.h`

Lightweight tab view model the chrome renders - a one-way projection of the host's authoritative workspace state. The host resolves its own state into these plain values on the main thread.

```cpp
struct alignas(64) Ui::tab_t final {
    id_t        id = INVALID_ID;    // stable identity, host-assigned
    std::string label;              // display text
    bool        isActive   = false; // currently focused tab
    bool        hasContent = false; // has a document (controls close button)
    bool        isLoading  = false; // content load in progress
    int         progress   = 0;     // loading sector index for the tab spinner
};
```

## Ui::TabBar

Header: `include/ui/tabbar.h`

The chrome's tab model: an ordered set of `tab_t` view models plus the scroll offset (which tab is leftmost-visible). Active-ness rides inside each `tab_t` so the underlying `Registry` stays active-agnostic. Scroll is the one piece of authoritative state owned here (pure chrome concern, no domain meaning).

```cpp
class Ui::TabBar final {
public:
    // read side (renderer)
    [[nodiscard]] const std::vector<id_t> & order() const;
    [[nodiscard]] const tab_t * find(id_t id) const;
    [[nodiscard]] std::size_t scrollOffset() const;
    [[nodiscard]] std::size_t count() const;
    [[nodiscard]] bool empty() const;

    // write side (host, main thread)
    void setTabs(std::vector<tab_t> tabs);                 // replace all; scroll clamped to new count
    void setLoading(id_t id, bool isLoading, int progress);// cheap in-place progress tick
    void scrollLeft();
    void scrollRight();
};
```

`setTabs` replaces the projected tabs in one shot (vector order = visual order) and clamps the preserved scroll offset to the new count. `setLoading` updates only the volatile loading fields in place (no rebuild/allocation), leaving structure/label/active untouched. `scrollLeft`/`scrollRight` step the offset within bounds.

## Ui::intent_t

Header: `include/ui/intent.h`

One thing the framework asks the host to do. A tagged record where `kind` selects which fields are meaningful.

```cpp
struct alignas(128) Ui::intent_t final {
    IntentKind  kind = IntentKind::EmitAction;
    id_t        id   = INVALID_ID;
    key_t       actionKey;
    std::string arg;

    bool operator==(const intent_t &) const = default;
};
```

Field usage by kind:

- `EmitAction` uses `actionKey` (+ optional `arg`).
- `OpenPopup` / `OpenSubmenu` / `OpenDialog` / `SwitchTab` / `CloseTab` use `id`.
- `CopyText` uses `arg`.
- `ClosePopup` uses none.

Id-only intents ship no geometry; the host resolves the screen anchor via `Context::bound(id)`.

```cpp
Ui::intent_t open;
open.kind = Ui::IntentKind::OpenPopup;
open.id   = menuId;

Ui::intent_t run{ Ui::IntentKind::EmitAction, Ui::INVALID_ID, "file:open", "" };
```

## Ui::IntentKind

Header: `include/ui/intentkind.h`

What the framework asks the host to do in response to input. The framework only emits these; the host executes them (dispatch a command, open a window, mutate workspaces, touch the OS). Anything the framework can do itself - layout, hover, tab scroll, menu highlight - is not an intent; it sets `result_t::isDirty` instead.

```cpp
enum class Ui::IntentKind : uint8_t {
    EmitAction,  // run a host command: actionKey (+ arg)
    OpenPopup,   // open the top-menu dropdown for a menu id
    ClosePopup,  // dismiss the open popup/submenu
    OpenSubmenu, // open/switch the submenu for an item id (hover-driven)
    OpenDialog,  // open the dialog attached to an item id
    SwitchTab,   // activate workspace/tab id
    CloseTab,    // close workspace/tab id
    CopyText,    // copy arg to the clipboard (status-bar text)
    ActivateRow, // dock row id selected
    ToggleRow,   // dock row id expand/collapse requested
};
```

Dock rows are host-projected, so both row kinds are host work: the row tree and its expanded flags live in the host's model, and it re-projects via `WindowManager::setDockRows` in response. `id` is the host's own row id, opaque to the framework - which is why it must be unique across docks.

Note what is NOT here: a dock slider's value. A drag is a continuous stream rather than a discrete request, so it reaches the host through `WindowManager::setOnRowValue` instead - see [windowing.md](windowing.md).

## Ui::result_t

Header: `include/ui/result.h`

Outcome of `Context::handleEvent`: whether the main UI needs a repaint, plus the intents the host must execute in order. `isDirty` alone (empty `intents`) means a framework-internal change happened (hover, tab scroll, menu highlight) that needs a repaint but no host action.

```cpp
struct alignas(32) Ui::result_t final {
    bool                  isDirty = false;
    std::vector<intent_t> intents;

    bool operator==(const result_t &) const = default;
};
```

### Intents-out vocabulary

`intent_t` / `IntentKind` / `result_t` are the framework's intents-OUT vocabulary: the framework turns raw input into a `result_t` carrying ordered `intent_t`s that describe what it wants done, without doing anything host-specific itself. They are the counterpart to the intents-IN execution seam `Ui::IChromeCommands` and the pure `Ui::routeIntent` switch, documented in [interfaces.md](interfaces.md) - the host (or `Ui::Shell`) consumes each `intent_t` and executes it there.

## Ui::Render::shadow_t

Header: `include/ui/render/shadow.h`

Drop-shadow parameters reused by any drawable that can cast one (rounded rects, images). Domain-blind (pure geometry + color, no UI state). A default-constructed value means "no shadow" (`opacity == 0`), so callers that do not want a shadow pass `{}`.

```cpp
struct alignas(32) Ui::Render::shadow_t final {
    fpx_t offsetX = 0;
    fpx_t offsetY = 0;
    fpx_t blur    = 0;
    fpx_t opacity = 0; // 0 = no shadow; multiplies color.a()
    Color color;

    [[nodiscard]] bool isVisible() const; // opacity > 0 && color.a() > 0
    bool operator==(const shadow_t &) const = default;
};
```

## See also

- [interfaces.md](interfaces.md) - the interface seams (`IRender`, `IEventApp`, `IRenderer`, `IChromeCommands`) and `routeIntent`.
- [render.md](render.md) - the render-chrome layer that consumes these value types.
- [README.md](README.md) - API documentation index.
