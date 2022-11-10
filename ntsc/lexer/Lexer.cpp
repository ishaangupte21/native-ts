#include "Lexer.h"
#include "Token.h"
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
    }
}

// This is the implementation of the method to scan single line comments from
// the source code. If we hit the end of the file, we will return false. If we
// hit a line terminator and need to keep lexing, we will return true.
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
            // If the current byte is an ASCII character, we can move past it.
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

            // Now that we have obtained the codepoint, we can check if it's a
            // Unicode Line Terminator.
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
}

// This is the implementation of the method to scan multi line comments from the
// source code. In this case, EOF would be an error, but we will still return
// false to recover. We will return true at the end of the comment.
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
            // If the asterisk is not followed by a slash, we will only consume
            // the asterisk.
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
                // Since we have already moved  the pointer forward, we can just
                // continue.
                continue;
            }

            // The Unicode Decoding has moved the pointer, so we only need to
            // move the column.
            ++col;
        }
    }
}

} // namespace ntsc