#include "Lexer.h"
#include "Token.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

auto main(int argc, char **argv) -> int {
    if (argc < 2) {
        llvm::errs() << llvm::raw_ostream::Colors::RED
                     << "fatal error: " << llvm::raw_ostream::Colors::WHITE
                     << " no source file given\n";
        return 1;
    }
    // For initial testing, we will use argv[1] as the source path.
    auto fileResult = llvm::MemoryBuffer::getFile(argv[1]);
    if (auto ec = fileResult.getError()) {
        llvm::errs() << llvm::raw_ostream::Colors::RED
                     << "fatal error: " << llvm::raw_ostream::Colors::WHITE
                     << argv[1] << ": " << ec.message() << '\n';
        return 1;
    }

    auto fileBuffer = fileResult->get();
    ntsc::Lexer lexer{fileBuffer->getBufferStart(), fileBuffer->getBufferEnd(),
                      argv[1]};
    ntsc::Token tok;

    lexer.lexToken(tok);

    llvm::outs() << "Token: " << static_cast<int>(tok.kind) << '\n';
}