// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureCreator. PureCreator is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureCreator is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureCreator. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file TestSanitize.cpp
 * @brief Adversarial unit tests for the Sanitize helpers.
 *
 * Each block tests one entry point with both positive (well-formed input
 * passes through intact) and negative (modelled real-world attacks /
 * malformed JSON / oversize payloads) cases. Stderr is captured per test
 * via the fixture so the asserted warning side-effects don't leak into
 * the test runner output.
 */

#include "common/sanitize.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>

using Common::Sanitize;

namespace {

// Fixture: every test redirects stderr through gtest's capture so log
// noise from sanitizers doesn't bleed into the runner. Tests that care
// about the warning side-effect call expectStderrContains(...) before
// the next sanitize call; the rest just discard.
class SanitizeTest : public ::testing::Test {
protected:
    void SetUp() override { ::testing::internal::CaptureStderr(); }
    void TearDown() override
    {
        if (!m_consumed) {
            (void)::testing::internal::GetCapturedStderr();
        }
    }

    std::string consumeStderr()
    {
        m_consumed = true;
        return ::testing::internal::GetCapturedStderr();
    }

    void expectStderrContains(const std::string & needle)
    {
        const std::string captured = consumeStderr();
        EXPECT_NE(captured.find(needle), std::string::npos)
        << "stderr did not contain expected fragment\nneedle: " << needle << "\ncaptured: " << captured;
        // Re-open capture so any follow-up sanitize call in the test still
        // gets its log siphoned away.
        ::testing::internal::CaptureStderr();
        m_consumed = false;
    }

private:
    bool m_consumed = false;
};

// Helper: build a string of every C0 control char (0x00..0x1F) plus DEL.
std::string allControlChars()
{
    std::string s;
    for (int i = 0; i < 0x20; ++i) {
        s.push_back(static_cast<char>(i));
    }
    s.push_back(static_cast<char>(0x7F));
    return s;
}

} // namespace

// ============================================================================
// Common::Sanitize::string - UI strings, 4 KB cap, keeps \n and \t
// ============================================================================

TEST_F(SanitizeTest, String_PlainAsciiPassesThrough)
{
    EXPECT_EQ(Common::Sanitize::string("Hello, world!", "label"), "Hello, world!");
}

TEST_F(SanitizeTest, String_EmptyStaysEmpty) { EXPECT_EQ(Common::Sanitize::string("", "label"), ""); }

TEST_F(SanitizeTest, String_NewlineAndTabPreserved)
{
    // UI strings are allowed to contain whitespace controls so that
    // multi-line locale entries and tabbed status messages survive.
    EXPECT_EQ(Common::Sanitize::string("line1\nline2\tcol", "label"), "line1\nline2\tcol");
}

TEST_F(SanitizeTest, String_Utf8PassesThrough)
{
    // High-byte UTF-8 sequences are not control chars (>= 0x80, != 0x7F),
    // so they survive the byte-level filter unchanged.
    const std::string kana = "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86"; // "あいう"
    EXPECT_EQ(Common::Sanitize::string(kana, "label"), kana);
}

TEST_F(SanitizeTest, String_ExactMaxLengthUnchanged)
{
    const std::string s(Common::Sanitize::MAX_STRING_LENGTH, 'a');
    EXPECT_EQ(Common::Sanitize::string(s, "label").size(), Common::Sanitize::MAX_STRING_LENGTH);
}

TEST_F(SanitizeTest, String_StripsAllC0ExceptNewlineAndTab)
{
    // Attack: log injection via embedded BEL/ESC/CR/NUL to forge log
    // lines, hijack a terminal, or split a JSON-emitted record. All
    // controls except \n and \t must be dropped, and the warning must
    // fire so operators can grep for tampering.
    // Adjacent string-literal trick: a hex escape consumes hex digits
    // greedily, so "\x03BAD" would be interpreted as one out-of-range
    // codepoint. Breaking the literal stops the escape at the boundary.
    const std::string input  = "ok\x01\x02\x03"
                               "BAD\x07\x0b\rEND";
    const std::string output = Common::Sanitize::string(input, "label");
    EXPECT_EQ(output, "okBADEND");
    expectStderrContains("Control characters stripped");
}

TEST_F(SanitizeTest, String_StripsDel)
{
    EXPECT_EQ(Common::Sanitize::string("a\x7f"
                                       "b",
                                       "label"),
              "ab");
}

TEST_F(SanitizeTest, String_StripsAnsiEscapeSequence)
{
    // ESC (0x1B) is the gateway byte for ANSI colour / cursor sequences
    // used in real-world terminal-injection attacks. Strip the ESC and
    // the rest of the sequence survives as harmless text, which is the
    // safest behaviour - the printable tail can no longer take effect.
    const std::string input  = "\x1b[31mDANGER\x1b[0m";
    const std::string output = Common::Sanitize::string(input, "label");
    EXPECT_EQ(output, "[31mDANGER[0m");
}

TEST_F(SanitizeTest, String_AllControlsCollapseToEmpty)
{
    // A payload that is nothing but control chars (minus \n / \t) must
    // collapse to empty - callers treat empty as "no value", which is
    // safer than a string of dropped characters concatenated.
    std::string input = allControlChars();
    // Remove \n and \t since those would survive.
    input.erase(std::remove(input.begin(), input.end(), '\n'), input.end());
    input.erase(std::remove(input.begin(), input.end(), '\t'), input.end());
    EXPECT_EQ(Common::Sanitize::string(input, "label"), "");
}

TEST_F(SanitizeTest, String_OversizeIsTruncated)
{
    // Length-based DoS: a multi-MB JSON value blowing up an unordered_map
    // key would let an attacker waste memory on every load. The cap is
    // 4 KB; anything over is truncated and logged.
    const std::string input(Common::Sanitize::MAX_STRING_LENGTH * 4, 'x');
    const std::string output = Common::Sanitize::string(input, "label");
    EXPECT_EQ(output.size(), Common::Sanitize::MAX_STRING_LENGTH);
    expectStderrContains("String truncated");
}

TEST_F(SanitizeTest, String_CustomMaxLengthRespected)
{
    EXPECT_EQ(Common::Sanitize::string("abcdef", "label", /*maxLength=*/3), "abc");
}

TEST_F(SanitizeTest, String_NulBytePresent_Stripped)
{
    // Attack: NUL-byte truncation in C-string interop. Strings that go
    // through Common::Sanitize::string and are later handed to a C API would
    // appear to end at the NUL. Stripping NUL keeps the std::string
    // length self-consistent.
    const std::string input("ok\0bad", 6);
    EXPECT_EQ(Common::Sanitize::string(input, "label"), "okbad");
}

// ============================================================================
// Common::Sanitize::filePath - relative resource names, 255 cap, no abs/no ..
// ============================================================================

TEST_F(SanitizeTest, FilePath_PlainNamePasses)
{
    EXPECT_EQ(Common::Sanitize::filePath("icon.svg", "icon"), "icon.svg");
}

TEST_F(SanitizeTest, FilePath_SubdirAllowed)
{
    EXPECT_EQ(Common::Sanitize::filePath("subdir/icon.svg", "icon"), "subdir/icon.svg");
}

TEST_F(SanitizeTest, FilePath_EmptyStaysEmpty) { EXPECT_EQ(Common::Sanitize::filePath("", "icon"), ""); }

TEST_F(SanitizeTest, FilePath_RejectsUnixAbsolute)
{
    // Attack: configuration trojaning - point a "icon" field at
    // /etc/passwd or /dev/urandom. Even a read-only file open is a
    // resource-exhaustion vector; an absolute path also leaks the
    // sandbox-confined "files live under res/" invariant.
    EXPECT_EQ(Common::Sanitize::filePath("/etc/passwd", "icon"), "");
    expectStderrContains("Absolute path rejected");
}

TEST_F(SanitizeTest, FilePath_RejectsWindowsAbsoluteBackslash)
{
    EXPECT_EQ(Common::Sanitize::filePath("\\\\server\\share\\evil.dll", "icon"), "");
    expectStderrContains("Absolute path rejected");
}

TEST_F(SanitizeTest, FilePath_RejectsTraversalDirect)
{
    // Attack: directory traversal - escape res/ with ../ to read or
    // overwrite files outside the sandbox.
    EXPECT_EQ(Common::Sanitize::filePath("../etc/passwd", "icon"), "");
    expectStderrContains("Path traversal rejected");
}

TEST_F(SanitizeTest, FilePath_RejectsEmbeddedTraversal)
{
    EXPECT_EQ(Common::Sanitize::filePath("a/b/../../../../etc/passwd", "icon"), "");
    expectStderrContains("Path traversal rejected");
}

TEST_F(SanitizeTest, FilePath_RejectsBareDoubleDot) { EXPECT_EQ(Common::Sanitize::filePath("..", "icon"), ""); }

TEST_F(SanitizeTest, FilePath_AllowsSingleDot)
{
    // Single dot is not a traversal token and is a legitimate way to say
    // "current directory" - keep it allowed so "./icon.svg" works.
    EXPECT_EQ(Common::Sanitize::filePath("./icon.svg", "icon"), "./icon.svg");
}

TEST_F(SanitizeTest, FilePath_StripsControlCharsBeforePolicyCheck)
{
    // Attack: smuggle absolute-path bytes past the prefix check by
    // sneaking a control char in front. Common::Sanitize::string runs first
    // and removes the control char; the policy check then sees the
    // real first character and rejects properly.
    EXPECT_EQ(Common::Sanitize::filePath("\x01/etc/passwd", "icon"), "");
    expectStderrContains("Absolute path rejected");
}

TEST_F(SanitizeTest, FilePath_LengthCappedAt255)
{
    // filePath delegates to string() with maxLength=255 so a long
    // attacker-controlled name can't be used to balloon any cache
    // keyed by the path.
    const std::string base(300, 'x');
    const std::string output = Common::Sanitize::filePath(base, "icon");
    EXPECT_EQ(output.size(), 255U);
}

// ============================================================================
// Common::Sanitize::fileContent - 64 KB cap, keeps \n and \t
// ============================================================================

TEST_F(SanitizeTest, FileContent_PlainTextPasses)
{
    EXPECT_EQ(Common::Sanitize::fileContent("hello\nworld\n", "x.txt"), "hello\nworld\n");
}

TEST_F(SanitizeTest, FileContent_TruncatedAtMaxFileSize)
{
    const std::string huge(Common::Sanitize::MAX_FILE_SIZE + 1024, 'a');
    EXPECT_EQ(Common::Sanitize::fileContent(huge, "x.txt").size(), Common::Sanitize::MAX_FILE_SIZE);
}

TEST_F(SanitizeTest, FileContent_StripsBinaryControls)
{
    // const char* literal can't be used directly: the implicit string_view
    // ctor uses strlen and would stop at the embedded NUL. Use explicit
    // size to keep the NUL byte in the input.
    const std::string input("ok\x00\x01\x02ok", 7);
    EXPECT_EQ(Common::Sanitize::fileContent(input, "x.txt"), "okok");
}

// ============================================================================
// Common::Sanitize::url - HTTPS-only allow-list
// ============================================================================

TEST_F(SanitizeTest, Url_HttpsPasses)
{
    EXPECT_EQ(Common::Sanitize::url("https://example.com/path?q=1", "link"), "https://example.com/path?q=1");
}

TEST_F(SanitizeTest, Url_RejectsPlainHttp)
{
    // Attack: downgrade to cleartext. Anything that isn't https:// is
    // refused; this is policy, not parsing.
    EXPECT_EQ(Common::Sanitize::url("http://example.com", "link"), "");
    expectStderrContains("Invalid URL");
}

TEST_F(SanitizeTest, Url_RejectsJavascriptScheme)
{
    // Attack: XSS via "javascript:alert(1)" smuggled through a UI link
    // field. Filtering on the literal "https://" prefix is the simplest
    // safe filter for the scheme list we actually support.
    EXPECT_EQ(Common::Sanitize::url("javascript:alert(1)", "link"), "");
}

TEST_F(SanitizeTest, Url_RejectsFtp) { EXPECT_EQ(Common::Sanitize::url("ftp://example.com", "link"), ""); }

TEST_F(SanitizeTest, Url_RejectsProtocolRelative) { EXPECT_EQ(Common::Sanitize::url("//example.com", "link"), ""); }

TEST_F(SanitizeTest, Url_RejectsMissingScheme) { EXPECT_EQ(Common::Sanitize::url("example.com", "link"), ""); }

TEST_F(SanitizeTest, Url_RejectsLeadingSpace)
{
    // Attack: " https://example.com" - the leading space slides the
    // "https://" off the start so the prefix check fails. Good - we
    // do not want to silently strip whitespace and accept the URL.
    EXPECT_EQ(Common::Sanitize::url(" https://example.com", "link"), "");
}

TEST_F(SanitizeTest, Url_RejectsEmpty) { EXPECT_EQ(Common::Sanitize::url("", "link"), ""); }

TEST_F(SanitizeTest, Url_TruncatesOversize)
{
    // Length DoS: refuse to keep a multi-MB URL in memory. Cap at 2 KB.
    const std::string huge = std::string("https://example.com/") + std::string(8192, 'x');
    const std::string out  = Common::Sanitize::url(huge, "link");
    ASSERT_LE(out.size(), Common::Sanitize::MAX_URL_LENGTH);
    EXPECT_EQ(out.rfind("https://", 0), 0U);
}

TEST_F(SanitizeTest, Url_StripsEmbeddedControlBytesBeforePrefixCheck)
{
    // Attack: smuggle an https-looking URL with control bytes that, on
    // a downstream consumer, might reassemble into a different URL.
    // Common::Sanitize::string runs first and the prefix check sees the cleaned
    // text; either it still starts with https:// (passes) or the prefix
    // is broken (rejected). Both outcomes are safe.
    EXPECT_EQ(Common::Sanitize::url("https://exa\x01mple.com", "link"), "https://example.com");
}

// ============================================================================
// Common::Sanitize::path - absolute FS paths, 4096 cap, strips ALL controls incl \n/\t
// ============================================================================

TEST_F(SanitizeTest, Path_UnixAbsolutePasses)
{
    EXPECT_EQ(Common::Sanitize::path("/home/user/file.step", "openFile"), "/home/user/file.step");
}

TEST_F(SanitizeTest, Path_WindowsAbsolutePasses)
{
    EXPECT_EQ(Common::Sanitize::path("C:\\Users\\file.step", "openFile"), "C:\\Users\\file.step");
}

TEST_F(SanitizeTest, Path_RootSlashPasses) { EXPECT_EQ(Common::Sanitize::path("/", "openFile"), "/"); }

TEST_F(SanitizeTest, Path_EmptyStaysEmpty) { EXPECT_EQ(Common::Sanitize::path("", "openFile"), ""); }

TEST_F(SanitizeTest, Path_AllowsDotDotForLegitimateAbsolute)
{
    // Unlike filePath, ".." is allowed here - on a real OS path it can
    // resolve through symlinks, mounts, and the user's home dir, all of
    // which are legitimate places a model file might live. Hygiene
    // (control chars, NUL) is enforced; structural policy is not.
    EXPECT_EQ(Common::Sanitize::path("/home/user/projects/../shared/m.step", "openFile"),
              "/home/user/projects/../shared/m.step");
}

TEST_F(SanitizeTest, Path_PreservesSpacesAndUnicode)
{
    const std::string in = "/home/User Name/\xe6\xa8\xa1\xe5\x9e\x8b.step"; // "/home/User Name/模型.step"
    EXPECT_EQ(Common::Sanitize::path(in, "openFile"), in);
}

TEST_F(SanitizeTest, Path_StripsNewlineAndTab)
{
    // Unlike Common::Sanitize::string, path is strict about ALL control bytes -
    // filesystem paths legitimately never contain a newline or tab, so
    // their presence is a sign of either a hand-edited session.json or
    // a copy-paste mishap; strip and log.
    const std::string out = Common::Sanitize::path("/home/\nuser/\tmodel.step", "openFile");
    EXPECT_EQ(out, "/home/user/model.step");
    expectStderrContains("Control characters stripped from path key");
}

TEST_F(SanitizeTest, Path_StripsAnsiEscapeBytes)
{
    EXPECT_EQ(Common::Sanitize::path("/home/\x1b[31muser/file.step", "openFile"), "/home/[31muser/file.step");
}

TEST_F(SanitizeTest, Path_RejectsEmbeddedNul)
{
    // Attack: NUL-byte truncation. On any C-string boundary (open(),
    // OCCT loaders, log lines, JSON dumps via a non-checked encoder)
    // an embedded NUL would re-cut the path. Reject the whole value
    // rather than silently keeping the prefix - the path is unsafe
    // regardless of what we'd "salvage".
    const std::string input("/home/user/\0/../../etc/passwd", 30);
    EXPECT_EQ(Common::Sanitize::path(input, "openFile"), "");
    expectStderrContains("Embedded NUL rejected");
}

TEST_F(SanitizeTest, Path_AllControlsCollapseToEmpty)
{
    // A path that is nothing but control bytes (no NUL) sanitises to
    // empty - which the caller treats as "no path", same as receiving
    // no value at all. Safer than yielding a magic "" path some C API
    // might interpret as "current dir".
    std::string input;
    for (int i = 1; i < 0x20; ++i) { // skip NUL: tested separately
        input.push_back(static_cast<char>(i));
    }
    input.push_back(static_cast<char>(0x7F));
    EXPECT_EQ(Common::Sanitize::path(input, "openFile"), "");
}

TEST_F(SanitizeTest, Path_TruncatedAtMaxPathLength)
{
    // Length DoS: an attacker-supplied 1 MB "/aaaa..." would explode
    // any path-keyed map. Cap at PATH_MAX (4096).
    const std::string huge = std::string("/") + std::string(8192, 'a');
    const std::string out  = Common::Sanitize::path(huge, "openFile");
    EXPECT_EQ(out.size(), Common::Sanitize::MAX_PATH_LENGTH);
    expectStderrContains("Path truncated");
}

TEST_F(SanitizeTest, Path_ExactMaxPathLengthUnchanged)
{
    const std::string at = std::string("/") + std::string(Common::Sanitize::MAX_PATH_LENGTH - 1, 'a');
    ASSERT_EQ(at.size(), Common::Sanitize::MAX_PATH_LENGTH);
    EXPECT_EQ(Common::Sanitize::path(at, "openFile").size(), Common::Sanitize::MAX_PATH_LENGTH);
}

TEST_F(SanitizeTest, Path_NulOnlyInputIsRejected)
{
    const std::string input("\0", 1);
    EXPECT_EQ(Common::Sanitize::path(input, "openFile"), "");
    expectStderrContains("Embedded NUL rejected");
}
