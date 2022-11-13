#ifndef NTSC_LEXER_H
#define NTSC_LEXER_H
#include "Token.h"

/*
    This file defines the Lexer interface for scanning TypeScript source files.
*/

namespace ntsc {
class Lexer {
    // These are the current pointer, buffer start pointer, and buffer end
    // pointer.
    char *ptr;
    const char *bufPtr, *endPtr;

    // This is a reference to the file path.
    // It is either from the CLI or from another source file.
    llvm::StringRef filePath;

    // This tracks whether the lexer has recovered from an error.
    bool lexerFailed = false;

    // These track the current line and column of the Lexer.
    int line = 1, col = 1;

    // This method will determine whether the given character is ASCII
    // horizontal whitespace according to the TypeScript standard.
    [[nodiscard]] static inline auto isHorizontalWhitespace(char c)
        -> bool const {
        return c == 0x9 || c == 0xb || c == 0xc || c == ' ';
    }

    // This method will determine whether the given character is an ASCII
    // character.
    [[nodiscard]] static inline auto isAscii(char c) -> bool const {
        return static_cast<uint8_t>(c) < 0x80;
    }

    // This method will scan Single Line comments from the source code
    [[nodiscard]] inline auto lexSingleLineComment(Token &tok,
                                                   bool afterLineTerminator)
        -> bool;

    // This method will scan Multi Line comments from the source code.
    [[nodiscard]] inline auto lexMultiLineComment(Token &tok,
                                                  bool &afterLineTerminator)
        -> bool;

    // This method will scan Numeric Literals from the source code. It will
    // begin with basic integer literals and then fork towards floating point
    // and big integer literals as needed.
    inline auto lexNumericLiteral(Token &tok, bool afterLineTerminator) -> void;

    // This method will scan Floating Point Literals after the floating point.
    inline auto lexFloatLiteral(Token &tok, char *startPtr, int startCol,
                                bool afterLineTerminator) -> void;

    // This method will diagnose errors related to unexpected null characters.
    // Since it will only be used locally in Lexer.cpp, the definition can be
    // done there.
    inline auto diagnoseUnexpectedNull() -> void;

    // This method will diagnose errors related to invalid UTF8 sequences. This
    // will be called when the LLVM conversion function fails. It will be
    // defined locally in Lexer.cpp.
    inline auto diagnoseInvalidUTF8() -> void;

    // This method will diagnose errors in the source when numeric seperators
    // are not followed by valid digits.
    inline auto diagnoseInvalidNumericSeparator() -> void;

  public:
    // This constructor will be used to instantiate Lexer instances with a start
    // pointer, end pointer, and a file path.
    Lexer(const char *bufPtr, const char *endPtr, llvm::StringRef filePath);

    // This method will return to the caller whether the Lexer has recovered
    // from one or more errors.
    [[nodiscard]] inline auto failed() -> bool const { return lexerFailed; }

    // This method is the main Lexer routine. It will take a reference to a
    // Token instance from the Parser and mutate that instance.
    auto lexToken(Token &tok) -> void;
};
}; // namespace ntsc

#endif