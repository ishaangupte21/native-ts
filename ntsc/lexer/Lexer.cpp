#include "Lexer.h"

namespace ntsc {
Lexer::Lexer(char *bufPtr, char *endPtr, llvm::StringRef filePath)
    : bufPtr{bufPtr}, endPtr{endPtr}, filePath{filePath} {
    // Here, we must check for the UTF-8 BOM.
    // If it exists, we will remove it.
    ptr = llvm::StringRef{bufPtr}.startswith("\xef\xbb\xbf") ? bufPtr + 3
                                                             : bufPtr;
}
} // namespace ntsc