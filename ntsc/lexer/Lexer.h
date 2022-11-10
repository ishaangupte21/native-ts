#ifndef NTSC_LEXER_H
#define NTSC_LEXER_H
#include "llvm/ADT/StringRef.h"

namespace ntsc {
class Lexer {
    // These are the current pointer, buffer start pointer, and buffer end
    // pointer.
    char *ptr, *bufPtr, *endPtr;

    // This is a reference to the file path.
    // It is either from the CLI or from another source file.
    llvm::StringRef filePath;

    // This tracks whether the lexer has recovered from an error.
    bool lexerFailed = false;

    // These track the current line and column of the Lexer.
    int line = 1, col = 1;

  public:
    Lexer(char *bufPtr, char *endPtr, llvm::StringRef filePath);
};
}; // namespace ntsc

#endif