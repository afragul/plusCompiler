#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Token types
typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER, 
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_OPERATOR,
    TOKEN_SEMICOLON,
    TOKEN_OPEN_BLOCK,
    TOKEN_CLOSE_BLOCK,
    TOKEN_EOF
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    char value[256];
    int line_number;
} Token;

// Parse tree node types
typedef enum {
    NODE_PROGRAM,
    NODE_DECLARATION,
    NODE_ASSIGNMENT,
    NODE_INCREMENT,
    NODE_DECREMENT,
    NODE_WRITE,
    NODE_LOOP,
    NODE_BLOCK,
    NODE_VARIABLE,
    NODE_NUMBER,
    NODE_STRING,
    NODE_NEWLINE
} NodeType;

// Parse tree node structure
typedef struct TreeNode {
    NodeType type;
    char* value;
    struct TreeNode* children[10];
    int child_count;
    int line_number;
} TreeNode;

// Global variables
const char* separators[] = {":=","-=", "+=",";","*","\""}; 
const int sep_count = 6;
const char* keywords[]={"number","repeat","times","write","and","newline"};  // Keywords dizisi tanımı 

Token tokens[1000];
int token_count = 0;
int current_token_index = 0;

int blockCount = 0;
int blockLines[100];
int blockLineIndex = 0;

#define MAX_VARS 200 
char declaredVariables[MAX_VARS][64];
int varCount = 0;

// Variable for interpreter
typedef struct {
    char name[64];
    long long value;
} Variable;

Variable variables[MAX_VARS];
int var_count = 0;

// Function prototypes
void addToken(TokenType type, const char* value, int line);
TreeNode* parseProgram();
TreeNode* parseStatement();
TreeNode* parseDeclaration();
TreeNode* parseAssignment();
TreeNode* parseWrite();
TreeNode* parseLoop();
TreeNode* createNode(NodeType type, char* value, int line);
void addChild(TreeNode* parent, TreeNode* child);
void printParseTree(TreeNode* node, int depth);
void executeProgram(TreeNode* node);
Token getCurrentToken();
Token peekNextToken();
void consumeToken();

// Lexer fonksiyonları
void replaceSeperator(char *line) {
    for (int i = 0; i < sep_count; i++) {
        char *pos = line;
        size_t sep_len = strlen(separators[i]);
        while ((pos = strstr(pos, separators[i])) != NULL) {
            memmove(pos + sep_len + 2, pos + sep_len, strlen(pos + sep_len) + 1);
            pos[0] = ' ';
            memcpy(pos + 1, separators[i], sep_len);
            pos[sep_len + 1] = ' ';
            pos += sep_len + 2;
        }
    }
}

int isNumber(const char *str) {
    char *endptr;
    if (str == NULL || *str == '\0')
        return 0;
    strtod(str, &endptr);
    return (*endptr == '\0');
}

// Variable management fonksiyonları
void addVariable(const char* var) {
    if (varCount < MAX_VARS) {
        strcpy(declaredVariables[varCount++], var);
    }
}

int isDeclared(const char* var) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(var, declaredVariables[i]) == 0) return 1;
    }
    return 0;
}

void addToken(TokenType type, const char* value, int line) {
    if (token_count < 1000) {
        tokens[token_count].type = type;
        strcpy(tokens[token_count].value, value);
        tokens[token_count].line_number = line;
        token_count++;
    }
}

// Blok kontrolleri ve identifier olarak işaretleme
void keywordType(char *type, int lineNumber) {
    if (strcmp(type, "number") == 0) { 
        addToken(TOKEN_KEYWORD, type, lineNumber);

        char* next = strtok(NULL, " \t\n");
        if (next && isalpha(next[0])) {
            addToken(TOKEN_IDENTIFIER, next, lineNumber);  // Fixed: Use 'next' instead of 'type'
            addVariable(next);
        } else {
            printf("Error on line %d: Invalid variable declaration after 'number'\n", lineNumber);
            exit(1);
        }
        return;
    }

    for (int i = 1; i < 6; i++) {
        if (strcmp(type, keywords[i]) == 0) {
            addToken(TOKEN_KEYWORD, type, lineNumber);
            return;
        }
    }

    if(type[0]=='{'){ 
        addToken(TOKEN_OPEN_BLOCK, "{", lineNumber);
        blockLines[blockLineIndex++] = lineNumber;
        blockCount++;
    } else if(type[0]=='}'){
        if (blockCount == 0) {
            printf("Error on line %d: Closing block without opening block!\n", lineNumber);
            exit(1);
        } else {
            blockCount--;
            blockLineIndex--;
            addToken(TOKEN_CLOSE_BLOCK, "}", lineNumber);
        }
    } else {
        if (!isDeclared(type)) {
            printf("Error on line %d: Undefine variable '%s'\n", lineNumber, type);
            exit(1);
        } else
        // Lexer aşamasında sadece identifier olarak işaretle
        // Semantic kontrolü parser aşamasında yap
        addToken(TOKEN_IDENTIFIER, type, lineNumber);
    }
}

// Parser helper functions
Token getCurrentToken() {
    if (current_token_index < token_count) {
        return tokens[current_token_index];
    }
    Token eof_token = {TOKEN_EOF, "", -1};
    return eof_token;
}

Token peekNextToken() {
    if (current_token_index + 1 < token_count) {
        return tokens[current_token_index + 1];
    }
    Token eof_token = {TOKEN_EOF, "", -1};
    return eof_token;
}

void consumeToken() {
    if (current_token_index < token_count) {
        current_token_index++;
    }
}

// Parse tree functions
TreeNode* createNode(NodeType type, char* value, int line) {
    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->child_count = 0;
    node->line_number = line;
    
    for(int i = 0; i < 10; i++) {
        node->children[i] = NULL;
    }
    
    return node;
}

void addChild(TreeNode* parent, TreeNode* child) {
    if(parent->child_count < 10) {
        parent->children[parent->child_count] = child;
        parent->child_count++;
    }
}

char* nodeTypeToString(NodeType type) {
    switch(type) {
        case NODE_PROGRAM: return "PROGRAM";
        case NODE_DECLARATION: return "DECLARATION";
        case NODE_ASSIGNMENT: return "ASSIGNMENT";
        case NODE_INCREMENT: return "INCREMENT";
        case NODE_DECREMENT: return "DECREMENT";
        case NODE_WRITE: return "WRITE";
        case NODE_LOOP: return "LOOP";
        case NODE_BLOCK: return "BLOCK";
        case NODE_VARIABLE: return "VARIABLE";
        case NODE_NUMBER: return "NUMBER";
        case NODE_STRING: return "STRING";
        case NODE_NEWLINE: return "NEWLINE";
        default: return "UNKNOWN";
    }
}

void printParseTree(TreeNode* node, int depth) {
    if(!node) return;
    
    for(int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    printf("%s", nodeTypeToString(node->type));
    if(node->value) {
        printf("(%s)", node->value);
    }
    printf("\n");
    
    for(int i = 0; i < node->child_count; i++) {
        printParseTree(node->children[i], depth + 1);
    }
}

// Parser functions
TreeNode* parseProgram() {
    TreeNode* program = createNode(NODE_PROGRAM, NULL, 1);
    
    while (getCurrentToken().type != TOKEN_EOF) {
        TreeNode* statement = parseStatement();
        if (statement) {
            addChild(program, statement);
        }
    }
    
    return program;
}

TreeNode* parseStatement() {
    Token current = getCurrentToken();
    
    if (current.type == TOKEN_KEYWORD) {
        if (strcmp(current.value, "number") == 0) {
            return parseDeclaration();
        } else if (strcmp(current.value, "write") == 0) {
            return parseWrite();
        } else if (strcmp(current.value, "repeat") == 0) {
            return parseLoop();
        }
    } else if (current.type == TOKEN_IDENTIFIER) {
        Token next = peekNextToken();
        if (next.type == TOKEN_OPERATOR) {
            if (strcmp(next.value, ":=") == 0) {
                return parseAssignment();
            }else if (strcmp(next.value, "+=") == 0) {
                TreeNode* increment = createNode(NODE_INCREMENT, NULL, current.line_number);
                TreeNode* var = createNode(NODE_VARIABLE, current.value, current.line_number);
                addChild(increment, var);
                consumeToken(); // variable
                consumeToken(); // +=

                Token value_token = getCurrentToken();
                TreeNode* value;
                if (value_token.type == TOKEN_NUMBER) {
                    value = createNode(NODE_NUMBER, value_token.value, value_token.line_number);
                } else if (value_token.type == TOKEN_IDENTIFIER) {
                    value = createNode(NODE_VARIABLE, value_token.value, value_token.line_number);
                } else {
                    printf("Error on line %d: Expected number or variable after '+='\n", value_token.line_number);
                    exit(1);
                }
                addChild(increment, value);
                consumeToken(); // value
                consumeToken(); // ;

                return increment;
            }else if (strcmp(next.value, "-=") == 0) {
                TreeNode* decrement = createNode(NODE_DECREMENT, NULL, current.line_number);
                TreeNode* var = createNode(NODE_VARIABLE, current.value, current.line_number);
                addChild(decrement, var);
                consumeToken(); // variable
                consumeToken(); // -=

                Token value_token = getCurrentToken();
                TreeNode* value;
                if (value_token.type == TOKEN_NUMBER) {
                    value = createNode(NODE_NUMBER, value_token.value, value_token.line_number);
                } else if (value_token.type == TOKEN_IDENTIFIER) {
                    value = createNode(NODE_VARIABLE, value_token.value, value_token.line_number);
                } else {
                    printf("Error on line %d: Expected number or variable after '-='\n", value_token.line_number);
                    exit(1);
                }
                addChild(decrement, value);
                consumeToken(); // value
                consumeToken(); // ;

                return decrement;
            }
        }
    }
    
    // Skip unknown tokens
    consumeToken();
    return NULL;
}

TreeNode* parseDeclaration() {
    TreeNode* decl = createNode(NODE_DECLARATION, NULL, getCurrentToken().line_number);
    
    consumeToken(); // "number"
    Token var_token = getCurrentToken();
    TreeNode* var = createNode(NODE_VARIABLE, var_token.value, var_token.line_number);
    addChild(decl, var);
    
    // REMOVED: Don't add variable again - it's already added in keywordType()
    // addVariable(var_token.value);
    
    consumeToken(); // variable name
    consumeToken(); // ;
    
    return decl;
}

TreeNode* parseAssignment() {
    TreeNode* assign = createNode(NODE_ASSIGNMENT, NULL, getCurrentToken().line_number);
    
    Token var_token = getCurrentToken();
    TreeNode* var = createNode(NODE_VARIABLE, var_token.value, var_token.line_number);
    addChild(assign, var);
    
    consumeToken(); // variable
    consumeToken(); // :=
    
    Token value_token = getCurrentToken();
    TreeNode* value;
    if (value_token.type == TOKEN_NUMBER) {
        value = createNode(NODE_NUMBER, value_token.value, value_token.line_number);
    } else {
        value = createNode(NODE_VARIABLE, value_token.value, value_token.line_number);
    }
    addChild(assign, value);
    
    consumeToken(); // value
    consumeToken(); // ;
    
    return assign;
}

TreeNode* parseWrite() {
    TreeNode* write_node = createNode(NODE_WRITE, NULL, getCurrentToken().line_number);
    
    consumeToken(); // "write"
    
    while (getCurrentToken().type != TOKEN_SEMICOLON && getCurrentToken().type != TOKEN_EOF) {
        Token current = getCurrentToken();
        
        if (current.type == TOKEN_STRING) {
            TreeNode* str = createNode(NODE_STRING, current.value, current.line_number);
            addChild(write_node, str);
        } else if (current.type == TOKEN_IDENTIFIER) {
            TreeNode* var = createNode(NODE_VARIABLE, current.value, current.line_number);
            addChild(write_node, var);
        } else if (current.type == TOKEN_KEYWORD && strcmp(current.value, "newline") == 0) {
            TreeNode* newline = createNode(NODE_NEWLINE, NULL, current.line_number);
            addChild(write_node, newline);
        }
        
        consumeToken();
        
        // Skip "and" keyword
        if (getCurrentToken().type == TOKEN_KEYWORD && strcmp(getCurrentToken().value, "and") == 0) {
            consumeToken();
        }
    }
    
    consumeToken(); // ;
    return write_node;
}

TreeNode* parseLoop() {
    TreeNode* loop = createNode(NODE_LOOP, NULL, getCurrentToken().line_number);
    
    consumeToken(); // "repeat"
    
    Token count_token = getCurrentToken();
    TreeNode* count;
    
    // Count hem sabit sayı hem de değişken olabilir
    if (count_token.type == TOKEN_NUMBER) {
        count = createNode(NODE_NUMBER, count_token.value, count_token.line_number);
    } else if (count_token.type == TOKEN_IDENTIFIER) {
        count = createNode(NODE_VARIABLE, count_token.value, count_token.line_number);
    } else {
        printf("Error on line %d: Expected number or variable after 'repeat'\n", count_token.line_number);
        exit(1);
    }
    
    addChild(loop, count);
    
    consumeToken(); // count
    consumeToken(); // "times"
    
    if (getCurrentToken().type == TOKEN_OPEN_BLOCK) {
        consumeToken(); // {
        TreeNode* block = createNode(NODE_BLOCK, NULL, getCurrentToken().line_number);
        
        while (getCurrentToken().type != TOKEN_CLOSE_BLOCK && getCurrentToken().type != TOKEN_EOF) {
            TreeNode* statement = parseStatement();
            if (statement) {
                addChild(block, statement);
            }
        }
        
        consumeToken(); // }
        addChild(loop, block);
    } else {
        TreeNode* statement = parseStatement();
        if (statement) {
            addChild(loop, statement);
        }
    }
    
    return loop;
}

// Simple interpreter functions
Variable* findVariable(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return &variables[i];
        }
    }
    return NULL;
}

Variable* createVariable(const char* name) {
    if (var_count < MAX_VARS) {
        strcpy(variables[var_count].name, name);
        variables[var_count].value = 0;
        return &variables[var_count++];
    }
    return NULL;
}

long long getValue(TreeNode* node) {
    if (node->type == NODE_NUMBER) {
        return atoll(node->value);
    } else if (node->type == NODE_VARIABLE) {
        Variable* var = findVariable(node->value);
        return var ? var->value : 0;
    }
    return 0;
}

void executeStatement(TreeNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_DECLARATION: {
            if (node->child_count > 0) {
                createVariable(node->children[0]->value);
            }
            break;
        }
        case NODE_ASSIGNMENT: {
            if (node->child_count >= 2) {
                Variable* var = findVariable(node->children[0]->value);
                if (!var) var = createVariable(node->children[0]->value);
                if (var) {
                    var->value = getValue(node->children[1]);
                }
            }
            break;
        }
        case NODE_INCREMENT: {
            if (node->child_count >= 2) {
                Variable* var = findVariable(node->children[0]->value);
                if (var) {
                    var->value += getValue(node->children[1]);
                }
            }
            break;
        }
        case NODE_DECREMENT: {
            if (node->child_count >= 2) {
                Variable* var = findVariable(node->children[0]->value);
                if (var) {
                    var->value -= getValue(node->children[1]);
                }
            }
            break;
        }
        case NODE_WRITE: {
            for (int i = 0; i < node->child_count; i++) {
                TreeNode* child = node->children[i];
                if (child->type == NODE_STRING) {
                    printf("%s", child->value);
                } else if (child->type == NODE_VARIABLE) {
                    Variable* var = findVariable(child->value);
                    printf("%lld", var ? var->value : 0);
                } else if (child->type == NODE_NEWLINE) {
                    printf("\n");
                }
            }
            break;
        }
        case NODE_LOOP: {
            if (node->child_count >= 1) {
                int count = (int)getValue(node->children[0]);
                for (int i = 0; i < count; i++) {
                    if (node->child_count > 1) {
                        executeStatement(node->children[1]);
                    }
                }
            }
            break;
        }
        case NODE_BLOCK: {
            for (int i = 0; i < node->child_count; i++) {
                executeStatement(node->children[i]);
            }
            break;
        }
        // Eksik olan durumlar için default case veya explicit cases
        case NODE_PROGRAM:
        case NODE_VARIABLE:
        case NODE_NUMBER:
        case NODE_STRING:
        case NODE_NEWLINE:
        default:
            // Bu node türleri executeStatement'te doğrudan işlenmez
            // Bunlar diğer node'ların çocukları olarak işlenir
            break;
    }
}

void executeProgram(TreeNode* program) {
    for (int i = 0; i < program->child_count; i++) {
        executeStatement(program->children[i]);
    }
}

int main(int argc, char *argv[]) {
    int lineControl = 0;
    
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    // Dosya açma ve hata kontrolü
    char inputFilename[256];
    snprintf(inputFilename, sizeof(inputFilename), "%s.ppp", argv[1]);
    FILE *dosya = fopen(inputFilename, "r");
    if (!dosya) {
        printf("File cannot be opened: %s\n", inputFilename);
        return 1;
    }

    char line[1024];
    int skipMode = 0;
    int strSkip = 0;
    
    // Satır satır okuma
    while (fgets(line, sizeof(line), dosya)) {
        lineControl++;
        replaceSeperator(line);
        
        if (skipMode) {
            printf("Error on line %d: Comment block is not closed!\n", lineControl-1);
            exit(1);
        }
        if (strSkip) {
            printf("Error on line %d: String block is not closed!\n", lineControl-1);
            exit(1);
        }
        // Tokenization işlemleri
        char *token = strtok(line, " \t\n");
        while (token != NULL) {
            if (strcmp(token, "*") == 0) {
                skipMode = !skipMode;
            }
            else if (!skipMode) {
                if (strcmp(token, "\"") == 0) {
                    char strConst[1024] = "";
                    token = strtok(NULL, " \t\n");
                    while (token != NULL && strcmp(token, "\"") != 0) {
                        if (strlen(strConst) > 0) strcat(strConst, " ");
                        strcat(strConst, token);
                        token = strtok(NULL, " \t\n");
                    }
                
                    if (token != NULL && strcmp(token, "\"") == 0) {
                        addToken(TOKEN_STRING, strConst, lineControl);
                        token = strtok(NULL, " \t\n");
                        continue;
                    } else {
                        printf("Error on line %d: String literal not closed with '\"'\n", lineControl);
                        exit(1);
                    }
                }else if (strcmp(token, ";") == 0) { 
                    addToken(TOKEN_SEMICOLON, ";", lineControl);
                }
                else {
                    int isOperator = 0;
                    for (int i = 0; i < sep_count; i++) {
                        if (strcmp(token, separators[i]) == 0 && strcmp(token, "*") != 0 && strcmp(token, "\"") != 0) { 
                            addToken(TOKEN_OPERATOR, token, lineControl);
                            isOperator = 1;
                            break;
                        }
                    }
                    if (!isOperator) {
                        if (isNumber(token)) {
                            addToken(TOKEN_NUMBER, token, lineControl);
                        } else {
                            keywordType(token, lineControl);
                        }
                    }
                }
            }
            token = strtok(NULL, " \t\n");
        }
    }

    fclose(dosya);

    if(blockCount > 0){
        for (int i = 0; i < blockLineIndex; i++) {
            printf("Error: Unclosed block opened on line %d\n", blockLines[i]);
            exit(1);
        }
    }

    // PARSER PHASE
    printf("=== PARSE TREE ===\n");
    TreeNode* parseTree = parseProgram();
    printParseTree(parseTree, 0);
    
    // INTERPRETER PHASE
    printf("\n=== PROGRAM OUTPUT ===\n");
    executeProgram(parseTree);
    
    return 0;
}