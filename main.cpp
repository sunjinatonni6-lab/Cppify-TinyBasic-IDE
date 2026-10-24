#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/*
  TinyBasic: a small educational compiler written in C++.
  Language:
    let x = 10;
    let y = x + 20;
    print y;
    print (y * 2) / 5;

  This compiler has its own lexer, parser, semantic checks, bytecode generator,
  and stack-based virtual machine. It does NOT call GCC/Clang to compile the
  user's TinyBasic source program.
*/

enum class TokenType {
    End, Number, Identifier,
    Let, Print,
    Plus, Minus, Star, Slash,
    Equal, Semicolon, LParen, RParen
};

struct Token {
    TokenType type;
    std::string text;
    int number = 0;
    int line = 1;
    int column = 1;
};

static std::string tokenName(TokenType t) {
    switch (t) {
        case TokenType::End: return "end of file";
        case TokenType::Number: return "number";
        case TokenType::Identifier: return "identifier";
        case TokenType::Let: return "'let'";
        case TokenType::Print: return "'print'";
        case TokenType::Plus: return "'+'";
        case TokenType::Minus: return "'-'";
        case TokenType::Star: return "'*'";
        case TokenType::Slash: return "'/'";
        case TokenType::Equal: return "'='";
        case TokenType::Semicolon: return "';'";
        case TokenType::LParen: return "'('";
        case TokenType::RParen: return "')'";
    }
    return "token";
}

class Lexer {
    const std::string& source;
    std::size_t pos = 0;
    int line = 1, column = 1;

    char peek() const { return pos < source.size() ? source[pos] : '\0'; }
    char advance() {
        char c = peek();
        if (c == '\n') { ++line; column = 1; }
        else { ++column; }
        if (pos < source.size()) ++pos;
        return c;
    }

public:
    explicit Lexer(const std::string& s) : source(s) {}

    std::vector<Token> scan() {
        std::vector<Token> out;
        while (peek() != '\0') {
            char c = peek();
            if (std::isspace(static_cast<unsigned char>(c))) { advance(); continue; }
            // Ignore single-line comments beginning with //.
            if (c == '/' && pos + 1 < source.size() && source[pos + 1] == '/') {
                while (peek() != '\n' && peek() != '\0') advance();
                continue;
            }

            int startLine = line, startCol = column;
            if (std::isdigit(static_cast<unsigned char>(c))) {
                std::string digits;
                while (std::isdigit(static_cast<unsigned char>(peek()))) digits += advance();
                out.push_back({TokenType::Number, digits, std::stoi(digits), startLine, startCol});
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                std::string word;
                while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')
                    word += advance();
                TokenType type = word == "let" ? TokenType::Let :
                                 word == "print" ? TokenType::Print : TokenType::Identifier;
                out.push_back({type, word, 0, startLine, startCol});
                continue;
            }

            TokenType type;
            switch (advance()) {
                case '+': type = TokenType::Plus; break;
                case '-': type = TokenType::Minus; break;
                case '*': type = TokenType::Star; break;
                case '/': type = TokenType::Slash; break;
                case '=': type = TokenType::Equal; break;
                case ';': type = TokenType::Semicolon; break;
                case '(': type = TokenType::LParen; break;
                case ')': type = TokenType::RParen; break;
                default:
                    throw std::runtime_error("Lexical error at line " +
                        std::to_string(startLine) + ", column " + std::to_string(startCol) +
                        ": unexpected character '" + c + "'");
            }
            out.push_back({type, std::string(1, c), 0, startLine, startCol});
        }
        out.push_back({TokenType::End, "", 0, line, column});
        return out;
    }
};

enum class OpCode { Push, Load, Store, Add, Sub, Mul, Div, Neg, Print, Halt };

struct Instruction {
    OpCode op;
    int operand = 0;
    std::string name;
};

static std::string opName(OpCode op) {
    switch (op) {
        case OpCode::Push: return "PUSH";
        case OpCode::Load: return "LOAD";
        case OpCode::Store: return "STORE";
        case OpCode::Add: return "ADD";
        case OpCode::Sub: return "SUB";
        case OpCode::Mul: return "MUL";
        case OpCode::Div: return "DIV";
        case OpCode::Neg: return "NEG";
        case OpCode::Print: return "PRINT";
        case OpCode::Halt: return "HALT";
    }
    return "UNKNOWN";
}

class Compiler {
    const std::vector<Token>& tokens;
    std::size_t pos = 0;
    std::vector<Instruction> code;
    std::unordered_map<std::string, int> variables;
    std::vector<std::string> variableNames;

    const Token& current() const { return tokens[pos]; }
    const Token& previous() const { return tokens[pos - 1]; }
    bool match(TokenType type) {
        if (current().type != type) return false;
        ++pos;
        return true;
    }
    const Token& consume(TokenType type, const std::string& message) {
        if (current().type != type) {
            throw std::runtime_error("Syntax error at line " + std::to_string(current().line) +
                ", column " + std::to_string(current().column) + ": " + message +
                " (found " + tokenName(current().type) + ")");
        }
        return tokens[pos++];
    }
    void emit(OpCode op, int operand = 0, std::string name = {}) {
        code.push_back({op, operand, std::move(name)});
    }
    int getVariable(const Token& name) {
        auto it = variables.find(name.text);
        if (it == variables.end()) {
            throw std::runtime_error("Semantic error at line " + std::to_string(name.line) +
                ": variable '" + name.text + "' is not declared");
        }
        return it->second;
    }

    void statement() {
        if (match(TokenType::Let)) {
            const Token& name = consume(TokenType::Identifier, "expected a variable name after 'let'");
            if (variables.count(name.text)) {
                throw std::runtime_error("Semantic error at line " + std::to_string(name.line) +
                    ": variable '" + name.text + "' is already declared");
            }
            consume(TokenType::Equal, "expected '=' after variable name");
            expression();
            consume(TokenType::Semicolon, "expected ';' after variable declaration");
            int slot = static_cast<int>(variableNames.size());
            variables[name.text] = slot;
            variableNames.push_back(name.text);
            emit(OpCode::Store, slot, name.text);
        } else if (match(TokenType::Print)) {
            expression();
            consume(TokenType::Semicolon, "expected ';' after print statement");
            emit(OpCode::Print);
        } else {
            throw std::runtime_error("Syntax error at line " + std::to_string(current().line) +
                ", column " + std::to_string(current().column) +
                ": expected 'let' or 'print' at start of statement");
        }
    }

    void expression() { term(); while (match(TokenType::Plus) || match(TokenType::Minus)) {
        TokenType op = previous().type; term(); emit(op == TokenType::Plus ? OpCode::Add : OpCode::Sub);
    }}
    void term() { unary(); while (match(TokenType::Star) || match(TokenType::Slash)) {
        TokenType op = previous().type; unary(); emit(op == TokenType::Star ? OpCode::Mul : OpCode::Div);
    }}
    void unary() {
        if (match(TokenType::Minus)) { unary(); emit(OpCode::Neg); }
        else primary();
    }
    void primary() {
        if (match(TokenType::Number)) { emit(OpCode::Push, previous().number); return; }
        if (match(TokenType::Identifier)) {
            const Token& name = previous();
            int slot = getVariable(name);
            emit(OpCode::Load, slot, name.text);
            return;
        }
        if (match(TokenType::LParen)) {
            expression();
            consume(TokenType::RParen, "expected ')' after expression");
            return;
        }
        throw std::runtime_error("Syntax error at line " + std::to_string(current().line) +
            ", column " + std::to_string(current().column) + ": expected a number, variable, or '('");
    }

public:
    explicit Compiler(const std::vector<Token>& t) : tokens(t) {}

    std::vector<Instruction> compile() {
        while (current().type != TokenType::End) statement();
        emit(OpCode::Halt);
        return code;
    }

    const std::vector<std::string>& getVariableNames() const { return variableNames; }
};

class VirtualMachine {
    const std::vector<Instruction>& code;
    std::vector<int> stack;
    std::vector<int> variables;
    std::ostream& output;

    int pop() {
        if (stack.empty()) throw std::runtime_error("Runtime error: stack underflow");
        int v = stack.back(); stack.pop_back(); return v;
    }
public:
    VirtualMachine(const std::vector<Instruction>& c, std::size_t variableCount, std::ostream& out)
        : code(c), variables(variableCount, 0), output(out) {}

    void run() {
        for (const auto& ins : code) {
            switch (ins.op) {
                case OpCode::Push: stack.push_back(ins.operand); break;
                case OpCode::Load:
                    if (ins.operand < 0 || static_cast<std::size_t>(ins.operand) >= variables.size())
                        throw std::runtime_error("Runtime error: invalid variable slot");
                    stack.push_back(variables[ins.operand]); break;
                case OpCode::Store: variables.at(ins.operand) = pop(); break;
                case OpCode::Add: { int b=pop(), a=pop(); stack.push_back(a+b); break; }
                case OpCode::Sub: { int b=pop(), a=pop(); stack.push_back(a-b); break; }
                case OpCode::Mul: { int b=pop(), a=pop(); stack.push_back(a*b); break; }
                case OpCode::Div: {
                    int b=pop(), a=pop();
                    if (b == 0) throw std::runtime_error("Runtime error: division by zero");
                    stack.push_back(a/b); break;
                }
                case OpCode::Neg: stack.push_back(-pop()); break;
                case OpCode::Print: output << pop() << '\n'; break;
                case OpCode::Halt: return;
            }
        }
    }
};

#include <emscripten/bind.h>

static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00"; out += hex[(c >> 4) & 0xf]; out += hex[c & 0xf];
                } else out += static_cast<char>(c);
        }
    }
    return out;
}

static std::string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::End: return "EOF";
        case TokenType::Number: return "NUMBER";
        case TokenType::Identifier: return "IDENTIFIER";
        case TokenType::Let: return "KEYWORD";
        case TokenType::Print: return "KEYWORD";
        case TokenType::Plus: case TokenType::Minus: case TokenType::Star: case TokenType::Slash: case TokenType::Equal: return "OPERATOR";
        case TokenType::Semicolon: case TokenType::LParen: case TokenType::RParen: return "PUNCTUATION";
    }
    return "TOKEN";
}

static std::string runTinyBasic(const std::string& source) {
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.scan();
        Compiler compiler(tokens);
        std::vector<Instruction> bytecode = compiler.compile();
        std::ostringstream output;
        VirtualMachine vm(bytecode, compiler.getVariableNames().size(), output);
        vm.run();

        std::ostringstream json;
        json << "{\"ok\":true,\"output\":\"" << jsonEscape(output.str()) << "\",\"tokens\":[";
        bool first = true;
        for (const auto& t : tokens) {
            if (t.type == TokenType::End) break;
            if (!first) json << ',';
            first = false;
            json << "{\"type\":\"" << tokenTypeName(t.type) << "\",\"value\":\"" << jsonEscape(t.text)
                 << "\",\"line\":" << t.line << ",\"column\":" << t.column << '}';
        }
        json << "],\"bytecode\":\"";
        for (std::size_t i = 0; i < bytecode.size(); ++i) {
            const auto& ins = bytecode[i];
            json << i << ": " << opName(ins.op);
            if (ins.op == OpCode::Push) json << ' ' << ins.operand;
            else if (ins.op == OpCode::Load || ins.op == OpCode::Store) {
                if (!ins.name.empty()) json << ' ' << ins.name << " (slot " << ins.operand << ')';
                else json << ' ' << ins.operand;
            }
            json << "\\n";
        }
        json << "\",\"instructionCount\":" << bytecode.size()
             << ",\"variableCount\":" << compiler.getVariableNames().size() << '}';
        return json.str();
    } catch (const std::exception& e) {
        return std::string("{\"ok\":false,\"error\":\"") + jsonEscape(e.what()) + "\",\"tokens\":[],\"bytecode\":\"\"}";
    }
}

EMSCRIPTEN_BINDINGS(cppify_tinybasic) {
    emscripten::function("runTinyBasic", &runTinyBasic);
}
