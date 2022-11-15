#ifndef NTSC_TOKEN_H
#define NTSC_TOKEN_H
#include "llvm/ADT/StringRef.h"

/*
    This file defines the Token and TokenKind interfaces.
    The Token Interface will be sent from the Lexer to the Parser
*/

namespace ntsc {

enum class TokenKind {
    FileEnd,

    // Punctuators
    LeftCurly,
    RightCurly,
    LeftParenthasis,
    RightParenthasis,
    LeftSquare,
    RightSquare,
    Dot,
    DotDotDot,
    Semicolon,
    Comma,
    Less,
    Greater,
    LessEquals,
    GreaterEquals,
    EqualsEquals,
    ExclaimationEquals,
    EqualsEqualsEquals,
    ExclaimationEqualsEquals,
    EqualsGreater,
    Plus,
    Minus,
    AsteriskAsterisk,
    Asterisk,
    Slash,
    Percent,
    PlusPlus,
    MinusMinus,
    LessLess,
    LessSlash,
    GreaterGreater,
    GreaterGreaterGreater,
    Ampersand,
    Bar,
    Caret,
    Exclaimation,
    Tilde,
    AmpersandAmpersand,
    BarBar,
    Question,
    QuestionQuestion,
    QuestionDot,
    Colon,
    Equals,
    PlusEquals,
    MinusEquals,
    AsteriskEquals,
    AsteriskAsteriskEquals,
    SlashEquals,
    PercentEquals,
    LessLessEquals,
    GreaterGreaterEquals,
    GreaterGreaterGreaterEquals,
    AmpersandEquals,
    BarEquals,
    CaretEquals,
    BarBarEquals,
    AmpersandAmpersandEquals,
    QuestionQuestionEquals,

    // Literals
    // We will add these special zero literals to eliminate the need for parsing
    // the value.
    ZeroLiteral,
    ZeroBigIntLiteral,
    DecimalLiteral,
    DecimalBigIntLiteral,
    FloatLiteral,
    HexLiteral,
    HexBigIntLiteral,
    OctalLiteral,
    OctalBigIntLiteral,
    BinaryLiteral,
    BinaryBigIntLiteral,
    StringLiteral
};

struct Token {
    TokenKind kind;
    int line, col;
    llvm::StringRef text;
    bool afterLineTerminator;

    inline auto set(TokenKind kind, int line, int col, bool afterLineTerminator)
        -> void {
        this->kind = kind;
        this->line = line;
        this->col = col;
        this->afterLineTerminator = afterLineTerminator;
    }

    inline auto set(TokenKind kind, int line, int col, bool afterLineTerminator,
                    llvm::StringRef text) -> void {
        this->kind = kind;
        this->line = line;
        this->col = col;
        this->afterLineTerminator = afterLineTerminator;
        this->text = text;
    }
};
}; // namespace ntsc

#endif