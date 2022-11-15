#include "Lexer.h"
#include "Token.h"
#include "UserOpts.h"
#include "llvm/Support/ConvertUTF.h"
#include "llvm/Support/raw_ostream.h"

/*
    This file implements the Lexer interface for scanning TypeScript Source
   Files.
*/

// This Macro Function will be used to inline UTF8 decoding.
#define decodeUTF8(x, y, z)                                                    \
    (llvm::convertUTF8Sequence((const llvm::UTF8 **)&x, (const llvm::UTF8 *)y, \
                               z, llvm::strictConversion))

// This Macro Function will be used to check whether a given CodePoint is a line
// terminator.
#define isUnicodeLT(x) (x == 0x2028 || x == 0x2029)

namespace ntsc {
// This is the implementation of the primary constructor.
Lexer::Lexer(const char *bufPtr, const char *endPtr, llvm::StringRef filePath)
    : bufPtr{bufPtr}, endPtr{endPtr}, filePath{filePath} {
    // Here, we must check for the UTF-8 BOM.
    // If it exists, we will remove it.
    ptr = const_cast<char *>(llvm::StringRef{bufPtr}.startswith("\xef\xbb\xbf")
                                 ? bufPtr + 3
                                 : bufPtr);
}

// This is the implementation of the inline method that diagnoses errors related
// to unexpected null characters in the source file. It will also update the
// Lexer's tracker for error recovery and move to the next character.
auto Lexer::diagnoseUnexpectedNull() -> void {
    llvm::errs() << llvm::raw_ostream::Colors::RED
                 << "error: " << llvm::raw_ostream::Colors::WHITE << filePath
                 << ": " << line << ":" << col
                 << ": unexpected null character in source\n";
    lexerFailed = true;
    ++line;
    ++col;
}

// This is the implementation of the function to diagnose UTF-8 sequence errors.
// It will update the lexer's tracker for error recovery. We will also skip the
// current character.
auto Lexer::diagnoseInvalidUTF8() -> void {
    llvm::errs() << llvm::raw_ostream::Colors::RED
                 << "error: " << llvm::raw_ostream::Colors::WHITE << filePath
                 << ": " << line << ":" << col
                 << ": invalid UTF-8 byte sequence\n";
    lexerFailed = true;
    ++ptr;
    // In this case, we will not increment the column because there is no
    // character!
}

// This is the implementation of the function to diagnose lexical errors when a
// numeric separator is not followed by a valid digit.
auto Lexer::diagnoseInvalidNumericSeparator() -> void {
    llvm::errs() << llvm::raw_ostream::Colors::RED
                 << "error: " << llvm::raw_ostream::Colors::WHITE << filePath
                 << ": " << line << ":" << ++col
                 << ": expected digit after numeric separator but found '"
                 << ptr[0] << "' instead\n";
    lexerFailed = true;

    // We don't need to move the pointer forward here.
}

// This is the implementation of the method to diagnose a lexical error when a
// Numeric Base specifier is not followed by a valid digit from that base.
auto Lexer::diagnoseMalformedRadixInt(const char type[], const char prefix[])
    -> void {
    llvm::errs() << llvm::raw_ostream::Colors::RED
                 << "error: " << llvm::raw_ostream::Colors::WHITE << filePath
                 << ": " << line << ":" << col << ": expected " << type
                 << " digit after prefix '" << prefix << "' but found '"
                 << ptr[0] << "' instead\n";
    lexerFailed = true;

    // Since this is the end of the literal, we don't need to move the pointer
    // forward.
}

// This is the implementation of the main Lexer routine. The goal is to scan the
// token and mutate the Token instance as quickly as possible.
// Since most code is unlikely to use Unicode codepoints, we will optimize the
// Lexer for ASCII and treat unicode as a special case rather than integrating
// UTF-8 decoding throughout.
auto Lexer::lexToken(Token &tok) -> void {
    // Since TypeScript allows Semicolon Insertion, we need to keep track of
    // whether the current token is preceded by a valid line terminator.
    bool afterLineTerminator = false;
beginLexer:
    // First, we must skip all horizontal whitespace.
    while (isHorizontalWhitespace(ptr[0])) {
        ++col;
        ++ptr;
    }

    // Now that we have removed horizontal whitespace, we can start simulating
    // the DFA for the Lexer. Line terminators will also be part of this DFA.
    switch (ptr[0]) {
    // First, we will handle potential EOFs.
    case 0:
        // We need to check if this is really the end of the file.
        if (ptr == endPtr) {
            tok.set(TokenKind::FileEnd, line, col, afterLineTerminator);
            return;
        }

        // If it isn't the end of the file, we need to diagnose the error, skip
        // the character, and then continue.
        diagnoseUnexpectedNull();
        goto beginLexer;

    // Next, we will handle line terminators.
    case '\n':
        ++line;
        col = 1;
        ++ptr;
        afterLineTerminator = true;
        goto beginLexer;
    case '\r':
        ++line;
        col = 1;
        afterLineTerminator = true;
        // According to the TypeScript standard, \r\n should be treated as a
        // single line terminator.
        if (ptr[1] == '\n')
            ptr += 2;
        else
            ++ptr;
        goto beginLexer;

    // Here, we will handle punctuators
    case '{':
        tok.set(TokenKind::LeftCurly, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '}':
        tok.set(TokenKind::RightCurly, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '(':
        tok.set(TokenKind::LeftParenthasis, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case ')':
        tok.set(TokenKind::RightParenthasis, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '[':
        tok.set(TokenKind::LeftSquare, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case ']':
        tok.set(TokenKind::RightSquare, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case ';':
        tok.set(TokenKind::Semicolon, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case ',':
        tok.set(TokenKind::Comma, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case ':':
        tok.set(TokenKind::Colon, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '.':
        // If we encounter only two dots, we must treat it as 2 separate tokens.
        if (ptr[1] == '.' && ptr[2] == '.') {
            tok.set(TokenKind::DotDotDot, line, col, afterLineTerminator);
            ptr += 3;
            col += 3;
            return;
        }

        tok.set(TokenKind::Dot, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '<':
        if (ptr[1] == '=') {
            tok.set(TokenKind::LessEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '<') {
            if (ptr[2] == '=') {
                tok.set(TokenKind::LessLessEquals, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }

            tok.set(TokenKind::LessLess, line, col, afterLineTerminator);
            col += 2;
            ptr += 2;
            return;
        }
        if (ptr[1] == '/') {
            tok.set(TokenKind::LessSlash, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Less, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '>':
        if (ptr[1] == '=') {
            tok.set(TokenKind::GreaterEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '>') {
            if (ptr[2] == '>') {
                if (ptr[3] == '=') {
                    tok.set(TokenKind::GreaterGreaterGreaterEquals, line, col,
                            afterLineTerminator);
                    ptr += 4;
                    col += 4;
                    return;
                }

                tok.set(TokenKind::GreaterGreaterGreater, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }
            if (ptr[2] == '=') {
                tok.set(TokenKind::GreaterGreaterEquals, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }

            tok.set(TokenKind::GreaterGreater, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Greater, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '=':
        if (ptr[1] == '=') {
            if (ptr[2] == '=') {
                tok.set(TokenKind::EqualsEqualsEquals, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }

            tok.set(TokenKind::EqualsEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '>') {
            tok.set(TokenKind::EqualsGreater, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Equals, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '!':
        if (ptr[1] == '=') {
            if (ptr[2] == '=') {
                tok.set(TokenKind::ExclaimationEqualsEquals, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }

            tok.set(TokenKind::ExclaimationEquals, line, col,
                    afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Exclaimation, line, col++, afterLineTerminator);
        ++ptr;
        return;

    case '+':
        if (ptr[1] == '+') {
            tok.set(TokenKind::PlusPlus, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '=') {
            tok.set(TokenKind::PlusEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Plus, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '-':
        if (ptr[1] == '-') {
            tok.set(TokenKind::MinusMinus, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '=') {
            tok.set(TokenKind::MinusEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Minus, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '*':
        if (ptr[1] == '*') {
            if (ptr[2] == '=') {
                tok.set(TokenKind::AsteriskAsteriskEquals, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }

            tok.set(TokenKind::AsteriskAsterisk, line, col,
                    afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '=') {
            tok.set(TokenKind::AsteriskEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Asterisk, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '/':
        switch (ptr[1]) {
        case '=':
            tok.set(TokenKind::SlashEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        case '/':
            // Single Line Comment
            if (lexSingleLineComment(tok, afterLineTerminator)) {
                // Since Single line comments are ended by line terminators, we
                // can automatically update afterLineTerminator.
                afterLineTerminator = true;
                goto beginLexer;
            }

            return;
        case '*':
            // Multi Line Comment
            if (lexMultiLineComment(tok, afterLineTerminator))
                goto beginLexer;

            return;
        default:
            tok.set(TokenKind::Slash, line, col++, afterLineTerminator);
            ++ptr;
            return;
        }
    case '%':
        if (ptr[1] == '=') {
            tok.set(TokenKind::PercentEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Percent, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '&':
        if (ptr[1] == '=') {
            tok.set(TokenKind::AmpersandEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '&') {
            if (ptr[2] == '=') {
                tok.set(TokenKind::AmpersandAmpersandEquals, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }

            tok.set(TokenKind::AmpersandAmpersand, line, col,
                    afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Ampersand, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '|':
        if (ptr[1] == '=') {
            tok.set(TokenKind::BarEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '|') {
            if (ptr[2] == '=') {
                tok.set(TokenKind::BarBarEquals, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }

            tok.set(TokenKind::BarBar, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Bar, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '^':
        if (ptr[1] == '=') {
            tok.set(TokenKind::CaretEquals, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Caret, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '~':
        tok.set(TokenKind::Tilde, line, col++, afterLineTerminator);
        ++ptr;
        return;
    case '?':
        if (ptr[1] == '?') {
            if (ptr[2] == '=') {
                tok.set(TokenKind::QuestionQuestionEquals, line, col,
                        afterLineTerminator);
                ptr += 3;
                col += 3;
                return;
            }

            tok.set(TokenKind::QuestionQuestion, line, col,
                    afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }
        if (ptr[1] == '.') {
            tok.set(TokenKind::QuestionDot, line, col, afterLineTerminator);
            ptr += 2;
            col += 2;
            return;
        }

        tok.set(TokenKind::Question, line, col++, afterLineTerminator);
        ++col;
        return;

    // Next, we will scan literals. We will begin with numeric literals.
    // '0' is the special literal character, so it has to be dealt with
    // separately.
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        lexNumericLiteral(tok, afterLineTerminator);
        return;

    case '0':
        switch (ptr[1]) {
        case '.':
            // lexFloatLiteral requires the pointer to be at the floating point
            lexFloatLiteral(tok, ptr++, col++, afterLineTerminator);
            return;
        // Hex Literal
        case 'x':
        case 'X':
            lexHexNumericLiteral(tok, afterLineTerminator);
            return;

        // Octal Literal
        case 'O':
        case 'o':
            lexOctalNumericLiteral(tok, afterLineTerminator);
            return;

        // Binary Literal
        case 'b':
        case 'B':
            lexBinaryNumericLiteral(tok, afterLineTerminator);
            return;

        // Zero BigInt literal
        case 'n':
            tok.set(TokenKind::ZeroBigIntLiteral, line, col,
                    afterLineTerminator);
            ptr += 2;
            col += 2;
            return;

        // Legacy Octal Literals
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            lexLegacyOctalLiteral(tok, afterLineTerminator);
            return;

        default:
            // Simple Zero Literal
            tok.set(TokenKind::ZeroLiteral, line, col, afterLineTerminator);
            return;
        }

    // String literals
    case '"':
        lexDoubleQuoteStrLiteral(tok, afterLineTerminator);
        return;

    case '\'':
        lexSingleQuoteStrLiteral(tok, afterLineTerminator);
        return;
    }
}

// This is the implementation of the method to scan single line comments
// from the source code. If we hit the end of the file, we will return
// false. If we hit a line terminator and need to keep lexing, we will
// return true.
auto Lexer::lexSingleLineComment(Token &tok, bool afterLineTerminator) -> bool {
    // First, we must consume the two slash characters.
    ptr += 2;
    col += 2;

    // Now, we will consume all characters until a line terminator is found.
    while (true) {
        switch (ptr[0]) {
        case 0:
            // We must check if this is really the end of the file.
            if (ptr == endPtr) {
                tok.set(TokenKind::FileEnd, line, col, afterLineTerminator);
                return false;
            }

            // Otherwise, we can treat it like a normal null character.
            diagnoseUnexpectedNull();
            continue;

        // Line terminators
        case '\n':
            ++line;
            col = 1;
            ++ptr;
            return true;
        case '\r':
            ++line;
            col = 1;
            if (ptr[1] == '\n')
                ptr += 2;
            else
                ++ptr;
            return true;
        default:
            // If the current byte is an ASCII character, we can move past
            // it.
            if (isAscii(ptr[0])) {
                ++ptr;
                ++col;
                continue;
            }

            llvm::UTF32 cp;
            if (decodeUTF8(ptr, endPtr, &cp) != llvm::conversionOK) {
                diagnoseInvalidUTF8();
                continue;
            }

            // Now that we have obtained the codepoint, we can check if it's
            // a Unicode Line Terminator.
            if (isUnicodeLT(cp)) {
                ++line;
                col = 1;
                return true;
            }

            // Otherwise, we can just continue. We have already moved the
            // pointer forward, so we must move the column forward.
            ++col;
        }
    }
} // namespace ntsc

// This is the implementation of the method to scan multi line comments from
// the source code. In this case, EOF would be an error, but we will still
// return false to recover. We will return true at the end of the comment.
auto Lexer::lexMultiLineComment(Token &tok, bool &afterLineTerminator) -> bool {
    // First, we need to move the pointer forward to consume the comment
    // opening.
    ptr += 2;
    col += 2;

    while (true) {
        switch (ptr[0]) {
        case '*':
            if (ptr[1] == '/') {
                // End of the comment
                ptr += 2;
                col += 2;
                return true;
            }
            // If the asterisk is not followed by a slash, we will only
            // consume the asterisk.
            ++ptr;
            ++col;
            continue;
        case 0:
            // We must check if this is really the end of the file.
            if (ptr == endPtr) {
                llvm::errs()
                    << llvm::raw_ostream::Colors::RED
                    << "error: " << llvm::raw_ostream::Colors::WHITE << filePath
                    << ": " << line << ":" << col
                    << ": unexpected end of file in multi line comment\n";
                tok.set(TokenKind::FileEnd, line, col, afterLineTerminator);
                return false;
            }

            // Otherwise, we can treat it like a regular null character.
            diagnoseUnexpectedNull();
            continue;
        case '\n':
            ++line;
            col = 1;
            ++ptr;
            afterLineTerminator = true;
            continue;
        case '\r':
            ++line;
            col = 1;
            afterLineTerminator = true;
            if (ptr[1] == '\n')
                ptr += 2;
            else
                ++ptr;
            continue;
        default:
            // If this is an ASCII character, we know for sure it's not a
            // Unicode Line Terminator.
            if (isAscii(ptr[0])) {
                ++ptr;
                ++col;
                continue;
            }

            llvm::UTF32 cp;

            // Now, we need to try to decode the UTF-8.
            if (decodeUTF8(ptr, endPtr, &cp) != llvm::conversionOK) {
                diagnoseInvalidUTF8();
                continue;
            }

            // Once we have the codepoint, we need to check if it's a line
            // terminator.
            if (isUnicodeLT(cp)) {
                ++line;
                col = 1;
                afterLineTerminator = true;
                // Since we have already moved  the pointer forward, we can
                // just continue.
                continue;
            }

            // The Unicode Decoding has moved the pointer, so we only need
            // to move the column.
            ++col;
        }
    }
}

#define isDigit(x) ((static_cast<uint32_t>(x) - '0') < 10)
#define isOctalDigit(x) ((static_cast<uint32_t>(x) - '0') < 8)
#define isBinaryDigit(x) (x == '0' || x == '1')
#define SIZE_T(x) (static_cast<size_t>(x))

// This is the implementation of the method which will scan numeric literals.
// The basic idea is to begin with simple integer literals and then if a
// floating point is found, we will fork to a floating point literal.
auto Lexer::lexNumericLiteral(Token &tok, bool afterLineTerminator) -> void {
    // Since we have already scanned the previous character, we can set the
    // start pointer and column to this position and then move forward.
    auto *startPtr = ptr++;
    auto startCol = col++;

    // All digits and numeric separators (underscores) will be part of the
    // current literal.
    while (true) {
        switch (ptr[0]) {
        // If we find a digit, we can just move forward.
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++ptr;
            ++col;
            continue;
        case '_':
            // If we find a Numeric Separator, it must be followed by a digit
            // according to the TypeScript standard.
            if (!isDigit(ptr[1])) {
                diagnoseInvalidNumericSeparator();
                // Since this is the end of the separator, we must end the
                // literal here. The placeholder literal, however, will not
                // contain the underscore.
                tok.set(TokenKind::DecimalLiteral, line, startCol,
                        afterLineTerminator,
                        {startPtr, SIZE_T(ptr - startPtr)});
                // Also, we need to consume the underscore.
                ++ptr;
                return;
            }

            // If we find a digit, we can just consume both the underscore and
            // the digit.
            ptr += 2;
            col += 2;
            continue;
        case 'n':
            // This is the delimeter for a BigInt literal.
            tok.set(TokenKind::DecimalBigIntLiteral, line, startCol,
                    // Here, we will actually omit the BigInt suffix from the
                    // Token lexeme to simplify the Integer parsing.
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            // We also need to consume the BigInt suffix.
            ++ptr;
            ++col;
            return;
        case '.':
            // Here, we need to fork the routine to scan Floating Point
            // literals.
            lexFloatLiteral(tok, startPtr, startCol, afterLineTerminator);
            return;
        default:
            // For all other characters, we can simply end the integer literal.
            tok.set(TokenKind::DecimalLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            return;
        }
    }
}

// This method is a fork from the primary lexNumericLiteral method to scan the
// back half of floating point literals.
auto Lexer::lexFloatLiteral(Token &tok, char *startPtr, int startCol,
                            bool afterLineTerminator) -> void {
    // First, we must consume the floating point and all of the optional digits.
    do {
        ++ptr;
        ++col;
    } while (isDigit(ptr[0]));

    // After, we need to scan the exponent portion.
    // If the current character is not an exponent prefix, we can end the
    // literal here.
    if (ptr[0] != 'e' && ptr[0] != 'E') {
        tok.set(TokenKind::FloatLiteral, line, startCol, afterLineTerminator,
                {startPtr, SIZE_T(ptr - startPtr)});
        return;
    }

    // We can also check for the exponent sign here and consume it if it exists.
    if (ptr[1] == '+' || ptr[1] == '-') {
        ptr += 2;
        col += 2;
    } else {
        ++ptr;
        ++col;
    }

    // Lastly, we need to consume the actual exponent, which is consists of
    // digits and numeric separators.
    while (true) {
        switch (ptr[0]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++ptr;
            ++col;
            continue;
        case '_':
            // If we find a Numeric Separator, it must be followed by a digit
            // according to the TypeScript standard.
            if (!isDigit(ptr[1])) {
                diagnoseInvalidNumericSeparator();
                // Since this is the end of the separator, we must end the
                // literal here. The placeholder literal, however, will not
                // contain the underscore.
                tok.set(TokenKind::FloatLiteral, line, startCol,
                        afterLineTerminator,
                        {startPtr, SIZE_T(ptr - startPtr)});
                // Also, we need to consume the underscore.
                ++ptr;
                return;
            }

            // If we find a digit, we can just consume both the underscore and
            // the digit.
            ptr += 2;
            col += 2;
            continue;
        default:
            // For all other characters, we will end the Float literal.
            tok.set(TokenKind::FloatLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            return;
        }
    }
}

// This is the implementation of the method that will scan Hexadecimal Numeric
// literals. It will consume all valid Hex Digits and numeric separators. If a
// bigint suffix is found, it will return a BigInt literal.
auto Lexer::lexHexNumericLiteral(Token &tok, bool afterLineTerminator) -> void {
    // Since the prefix cannot be part of the Token lexeme, we must consume it.
    auto startCol = col;
    ptr += 2;
    col += 2;
    auto *startPtr = ptr;

    // The next character must be a Hex Digit
    if (!isHexDigit(ptr[0])) {
        diagnoseMalformedRadixInt("hexadecimal", "0x");

        // For placeholder purposes, we will return a zero literal.
        tok.set(TokenKind::ZeroLiteral, line, startCol, afterLineTerminator);
        return;
    }

    // Since we have one hex digit for sure, we can consume it.
    ++ptr;
    ++col;

    // Now, we can consume all hex digits and numeric separators.
    while (true) {
        switch (ptr[0]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            ++ptr;
            ++col;
            continue;
        case '_':
            // We must check if the numeric separator is followed by a valid Hex
            // Digit.
            if (!isHexDigit(ptr[1])) {
                diagnoseInvalidNumericSeparator();

                // Now, we must mark this as the end of the literal. The
                // underscore will be ommited from the literal text.
                tok.set(TokenKind::HexLiteral, line, startCol,
                        afterLineTerminator,
                        {startPtr, SIZE_T(ptr - startPtr)});
                // Finally, we need to consume the underscore
                ++ptr;
                ++col;
                return;
            }

            // If there is a hex digit, we have pre-scanned it and we can
            // consume it.
            ptr += 2;
            col += 2;
            continue;
        case 'n':
            // Big Int literal suffix
            tok.set(TokenKind::HexBigIntLiteral, line, startCol,
                    // The BigInt suffix must be ommited  from the token lexeme.
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            // We also need to consume the big int suffix
            ++ptr;
            ++col;
            return;
        default:
            // For all other characters, we will mark it as the end for Integer
            // Literals.
            tok.set(TokenKind::HexLiteral, line, startCol, afterLineTerminator,
                    {startPtr, SIZE_T(ptr - startPtr)});
            return;
        }
    }
}

// This is the implementation of the method to scan Octal Numeric Literals. We
// must consume all valid Octal Digits, Numeric Separators, and the Big Int
// suffix.
auto Lexer::lexOctalNumericLiteral(Token &tok, bool afterLineTerminator)
    -> void {
    // First, we need to consume the prefix.
    auto startCol = col;
    ptr += 2;
    col += 2;
    auto *startPtr = ptr;

    // The first digit must be a valid octal digit.
    if (!isOctalDigit(ptr[0])) {
        diagnoseMalformedRadixInt("octal", "0o");

        // As a placeholder, we will return the zero literal.
        tok.set(TokenKind::ZeroLiteral, line, startCol, afterLineTerminator);
        return;
    }

    // We have one octal digit for sure, so we can consume it.
    ++ptr;
    ++col;

    // Now, we need to consume all hex digits and seperators.
    while (true) {
        switch (ptr[0]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            ++ptr;
            ++col;
            continue;
        case '_':
            // Numeric Separator must be followed by an octal digit.
            if (!isOctalDigit(ptr[1])) {
                diagnoseInvalidNumericSeparator();

                tok.set(TokenKind::OctalLiteral, line, startCol,
                        afterLineTerminator,
                        {startPtr, SIZE_T(ptr - startPtr)});
                // Consume underscore
                ++ptr;
                ++col;
                return;
            }

            // We have a valid octal digit for sure, so consume both.
            ptr += 2;
            col += 2;
            continue;
        case 'n':
            // Big Int suffix
            tok.set(TokenKind::OctalBigIntLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            // Consume suffix
            ++ptr;
            ++col;
            return;
        default:
            // End of the literal.
            tok.set(TokenKind::OctalLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            return;
        }
    }
}

// This is the implementation of the method to scan Binary Numeric Literals. It
// will consume all binary digits, numeric separators, and the bigint suffix.
auto Lexer::lexBinaryNumericLiteral(Token &tok, bool afterLineTerminator)
    -> void {
    // First, we need to consume the prefix.
    auto startCol = col;
    ptr += 2;
    col += 2;
    auto *startPtr = ptr;

    // The first character must be a binary digit.
    if (!isBinaryDigit(ptr[0])) {
        diagnoseMalformedRadixInt("binary", "0b");

        // Placeholder 0 literal.
        tok.set(TokenKind::ZeroLiteral, line, startCol, afterLineTerminator);
        return;
    }

    // We know there is a binary digit, so we can consume it.
    ++ptr;
    ++col;

    // Now we need to consume all digits and separators.
    while (true) {
        switch (ptr[0]) {
        case '0':
        case '1':
            ++ptr;
            ++col;
            continue;
        case '_':
            // Numeric separators must be followed by a binary digit.
            if (!isBinaryDigit(ptr[1])) {
                diagnoseInvalidNumericSeparator();

                tok.set(TokenKind::BinaryLiteral, line, startCol,
                        afterLineTerminator,
                        {startPtr, SIZE_T(ptr - startPtr)});
                // Consume underscore
                ++ptr;
                ++col;
                return;
            }

            // We have 2 valid characters, so we can consume both.
            ptr += 2;
            col += 2;
            continue;
        case 'n':
            // BigInt literal
            tok.set(TokenKind::BinaryBigIntLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            // Consume suffix
            ++ptr;
            ++col;
            return;
        default:
            // End of the regular binary literal.
            tok.set(TokenKind::BinaryLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            return;
        }
    }
}

// This is the implementation of the method to scan Legacy Octal Literals. At
// the end we must check if strict mode is enabled. If it is, using this literal
// will be an error.
auto Lexer::lexLegacyOctalLiteral(Token &tok, bool afterLineTerminator)
    -> void {
    // First, we need to move the column ahead for the prefix.
    auto *startPtr = ++ptr;
    auto startCol = col;

    // Since we have an octal digit for sure, we can consume it.
    ++ptr;
    col += 2;

    // Now, we need to consume all octal digits and numeric separatators.
    while (true) {
        switch (ptr[0]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            ++ptr;
            ++col;
            continue;
        case '_':
            // Numeric Separator must be followed by an octal digit.
            if (!isOctalDigit(ptr[1])) {
                diagnoseInvalidNumericSeparator();

                tok.set(TokenKind::OctalLiteral, line, startCol,
                        afterLineTerminator,
                        {startPtr, SIZE_T(ptr - startPtr)});
                // Consume underscore
                ++ptr;
                ++col;
                return;
            }

            // We have a valid octal digit for sure, so consume both.
            ptr += 2;
            col += 2;
            continue;
        // Legacy literals cannot have the bigint suffix.
        default:
            tok.set(TokenKind::OctalLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});
            // Here, we must check if Strict mode is enabled.
            if (UserOpts::strictModeEnabled) {
                llvm::errs() << llvm::raw_ostream::Colors::RED
                             << "error: " << llvm::raw_ostream::Colors::WHITE
                             << filePath << ": " << line << ":" << startCol
                             << ": legacy octal literals are not permitted in "
                                "strict mode. Consider using the prefix '0o' "
                                "or pass the argument '-no-strict-mode'\n";
                lexerFailed = true;
            }
            return;
        }
    }
}

// This is the implementation of the method to scan double quote string
// literals. First, we will scan only ascii and if needed, we will switch to
// unicode.
auto Lexer::lexDoubleQuoteStrLiteral(Token &tok, bool afterLineTerminator)
    -> void {
    // First, we will move the pointer past the quote.
    // The column should begin at the quote, but the text should not contain the
    // quote.
    auto *startPtr = ++ptr;
    auto startCol = col++;

    // Now, we need to consume all ASCII characters, and if unicode is found, we
    // should switch to unicode.
    while (true) {
        if (ptr[0] == '"') {
            // End of the double quoted string.
            tok.set(TokenKind::StringLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});

            // Consume the quote
            ++ptr;
            ++col;
            return;
        }
        // If its ASCII, we can just move forward.
        if (isAscii(ptr[0])) {
            ++ptr;
            ++col;
            continue;
        }

        llvm::UTF32 cp;
        if (decodeUTF8(ptr, endPtr, &cp) != llvm::conversionOK) {
            // Conversion failure
            diagnoseInvalidUTF8();
            // Treat this character like it didn't exist.
        } else {
            // Since the decoding function handles the pointer, we just need to
            // increment the column
            ++col;
        }
    }
}

// This is the implementation of the method to scan single quote string
// literals. Similar to double quotes, we will begin with ascii and then do
// unicode if needed.
auto Lexer::lexSingleQuoteStrLiteral(Token &tok, bool afterLineTerminator)
    -> void {
    // First, we will move the pointer past the quote.
    // The column should begin at the quote, but the text should not contain the
    // quote.
    auto *startPtr = ++ptr;
    auto startCol = col++;

    // Now, we need to consume all ASCII characters, and if unicode is found, we
    // should switch to unicode.
    while (true) {
        if (ptr[0] == '\'') {
            // End of the single quoted string.
            tok.set(TokenKind::StringLiteral, line, startCol,
                    afterLineTerminator, {startPtr, SIZE_T(ptr - startPtr)});

            // Consume the quote
            ++ptr;
            ++col;
            return;
        }
        // If its ASCII, we can just move forward.
        if (isAscii(ptr[0])) {
            ++ptr;
            ++col;
            continue;
        }

        llvm::UTF32 cp;
        if (decodeUTF8(ptr, endPtr, &cp) != llvm::conversionOK) {
            // Conversion failure
            diagnoseInvalidUTF8();
            // Treat this character like it didn't exist.
        } else {
            // Since the decoding function handles the pointer, we just need to
            // increment the column
            ++col;
        }
    }
}
} // namespace ntsc