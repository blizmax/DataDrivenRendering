#pragma once

#include <stdio.h>
#include <vector>

//
// INTERFACE
//

// Simple string that references another one. Used to not allocate strings if not needed.
struct StringRef {

    size_t                          length;
    char*                           text;

}; // struct StringRef

bool                                equals( const StringRef& a, const StringRef& b );
void                                copy( const StringRef& a, char* buffer, uint32_t buffer_size );



//
// Class that preallocates a buffer and appends strings to it. Reserve an additional byte for the null termination.
struct StringBuffer {

    void init( uint32_t size ) {
        if ( data ) {
            free( data );
        }

        if ( size < 1 ) {
            printf( "ERROR: Buffer cannot be empty!\n" );
            return;
        }

        data = (char*)malloc( size + 1 );
        buffer_size = size;
        current_size = 0;
    }

    void append( const char* format, ... ) {
        if ( current_size >= buffer_size ) {
            printf( "Buffer full! Please allocate more size.\n" );
            return;
        }

        va_list args;
        va_start( args, format );
        int written_chars = vsnprintf_s( &data[current_size], buffer_size - current_size, _TRUNCATE, format, args );
        current_size += written_chars > 0 ? written_chars : 0;
        va_end( args );
    }

    void append( char c ) {
        if ( current_size + 1 == buffer_size )
            return;

        data[current_size++] = c;
        data[current_size] = 0;
    }

    void append( const StringRef& text ) {
        const uint32_t max_length = current_size + text.length < buffer_size ? text.length : buffer_size - current_size;
        if ( max_length == 0 || max_length >= buffer_size ) {
            printf( "Buffer full! Please allocate more size.\n" );
            return;
        }

        memcpy( &data[current_size], text.text, max_length );
        current_size += max_length;

        // Add null termination for string.
        // By allocating one extra character for the null termination this is always safe to do.
        data[current_size] = 0;
    }

    void clear() {
        current_size = 0;
    }

    char*                           data            = nullptr;
    uint32_t                        buffer_size     = 1024;
    uint32_t                        current_size    = 0;

}; // struct StringBuffer


//
//
struct Token {

    enum Type {
        Token_Unknown,

        Token_OpenParen,
        Token_CloseParen,
        Token_Colon,
        Token_Semicolon,
        Token_Asterisk,
        Token_OpenBracket,
        Token_CloseBracket,
        Token_OpenBrace,
        Token_CloseBrace,
        Token_OpenAngleBracket,
        Token_CloseAngleBracket,

        Token_String,
        Token_Identifier,
        Token_Number,

        Token_EndOfStream,
    }; // enum Type

    Type                            type;
    StringRef                       text;
    uint32_t                        line;

}; // struct Token

//
// The role of the Lexer is to divide the input string into a list of Tokens.
struct Lexer {
    
    char*                           position            = nullptr;    
    uint32_t                        line                = 0;
    uint32_t                        column              = 0;

    bool                            error               = false;
    uint32_t                        error_line          = 0;

}; // struct Lexer

//
// Lexer-related methods
//
static void                         initLexer( Lexer* lexer, char* text );
static void                         nextToken( Lexer* lexer, Token& out_token );    // Retrieve the next token. Most importand method!
static void                         skipWhitespace( Lexer* lexer );

static bool                         equalToken( Lexer* lexer, Token& out_token, Token::Type expected_type );    // Check for token, no error if different
static bool                         expectToken( Lexer* lexer, Token& out_token, Token::Type expected_type );   // Expect a token, error if not present

//
// Define the language specific structures.
namespace ast {

    struct Type {

        enum Types {
            Types_Primitive, Types_Enum, Types_Struct, Types_Command, Types_None
        };

        enum PrimitiveTypes {
            Primitive_Int32, Primitive_Uint32, Primitive_Int16, Primitive_Uint16, Primitive_Int8, Primitive_Uint8, Primitive_Int64, Primitive_Uint64, Primitive_Float, Primitive_Double, Primitive_Bool, Primitive_None
        };

        Types                       type;
        PrimitiveTypes              primitive_type;
        StringRef                   name;

        std::vector<StringRef>      names;
        std::vector<const Type*>    types;
        //std::vector<Attribute> attributes;
        bool                        exportable = true;

    }; // struct Type

} // namespace ast

//
// The Parser parses Tokens using the Lexer and generate an Abstract Syntax Tree.
struct Parser {

    Lexer*                          lexer               = nullptr;

    ast::Type*                      types               = nullptr;
    uint32_t                        types_count         = 0;
    uint32_t                        types_max           = 0;

}; // struct Parser

static void                         initParser( Parser* parser, Lexer* lexer, uint32_t max_types );
static void                         generateAST( Parser* parser );

static void                         identifier( Parser* parser, const Token& token );
static const ast::Type*             findType( Parser* parser, const StringRef& name );

static void                         declarationStruct( Parser* parser );        // Follows the syntax 'struct name { (member)* };
static void                         declarationEnum( Parser* parser );
static void                         declarationCommand( Parser* parser );
static void                         declarationVariable( Parser* parser, const StringRef& type_name, ast::Type& type );

//
// Given an AST the CodeGenerator will create the output code.
struct CodeGenerator {

    const Parser*                   parser              = nullptr;
    StringBuffer                    string_buffer_0;
    StringBuffer                    string_buffer_1;
    StringBuffer                    string_buffer_2;

    bool                            generate_imgui      = false;

}; // struct CodeGenerator

static void                         initCodeGenerator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size );
static void                         generateCode( CodeGenerator* code_generator, const char* filename );

static void                         outputCPPStruct( CodeGenerator* code_generator, FILE* output, const ast::Type& type );
static void                         outputCPPEnum( CodeGenerator* code_generator, FILE* output, const ast::Type& type );
static void                         outputCPPCommand( CodeGenerator* code_generator, FILE* output, const ast::Type& type );

//
// IMPLEMENTATION
//

inline bool equals( const StringRef& a, const StringRef& b ) {
    if ( a.length != b.length )
        return false;

    for ( uint32_t i = 0; i < a.length; ++i ) {
        if ( a.text[i] != b.text[i] ) {
            return false;
        }
    }

    return true;
}

inline void copy( const StringRef& a, char* buffer, uint32_t buffer_size ) {
    const uint32_t max_length = buffer_size - 1 < a.length ? buffer_size - 1 : a.length;
    memcpy( buffer, a.text, max_length );
    buffer[a.length] = 0;
}

//
// Lexer
void initLexer( Lexer* lexer, char* text ) {
    lexer->position = text;
    lexer->line = 1;
    lexer->column = 0;
}

inline bool IsEndOfLine( char c ) {
    bool Result = ((c == '\n') || (c == '\r'));
    return(Result);
}

inline bool IsWhitespace( char c ) {
    bool Result = ((c == ' ') || (c == '\t') || (c == '\v') || (c == '\f') || IsEndOfLine( c ));
    return(Result);
}

inline bool IsAlpha( char c ) {
    bool Result = (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
    return(Result);
}

inline bool IsNumber( char c ) {
    bool Result = ((c >= '0') && (c <= '9'));
    return(Result);
}

void nextToken( Lexer* lexer, Token& token ) {

    // Skip all whitespace first so that the token is without them.
    skipWhitespace( lexer );

    // Initialize token
    token.type = Token::Token_Unknown;
    token.text.text = lexer->position;
    token.text.length = 1;
    token.line = lexer->line;

    char c = lexer->position[0];
    ++lexer->position;

    switch ( c ) {
        case '\0':
        {
            token.type = Token::Token_EndOfStream;
        } break;
        case '(':
        {
            token.type = Token::Token_OpenParen;
        } break;
        case ')':
        {
            token.type = Token::Token_CloseParen;
        } break;
        case ':':
        {
            token.type = Token::Token_Colon;
        } break;
        case ';':
        {
            token.type = Token::Token_Semicolon;
        } break;
        case '*':
        {
            token.type = Token::Token_Asterisk;
        } break;
        case '[':
        {
            token.type = Token::Token_OpenBracket;
        } break;
        case ']':
        {
            token.type = Token::Token_CloseBracket;
        } break;
        case '{':
        {
            token.type = Token::Token_OpenBrace;
        } break;
        case '}':
        {
            token.type = Token::Token_CloseBrace;
        } break;

        case '"':
        {
            token.type = Token::Token_String;

            token.text.text = lexer->position;

            while ( lexer->position[0] &&
                    lexer->position[0] != '"' )
            {
                if ( (lexer->position[0] == '\\') &&
                     lexer->position[1] )
                {
                    ++lexer->position;
                }
                ++lexer->position;
            }

            token.text.length = lexer->position - token.text.text;
            if ( lexer->position[0] == '"' ) {
                ++lexer->position;
            }
        } break;

        default:
        {
            // Identifier/keywords
            if ( IsAlpha( c ) ) {
                token.type = Token::Token_Identifier;

                while ( IsAlpha( lexer->position[0] ) || IsNumber( lexer->position[0] ) || (lexer->position[0] == '_') ) {
                    ++lexer->position;
                }

                token.text.length = lexer->position - token.text.text;
            } // Numbers
            else if ( IsNumber( c ) ) {
                token.type = Token::Token_Number;
            }
            else {
                token.type = Token::Token_Unknown;
            }
        } break;
    }
}

void skipWhitespace( Lexer* lexer ) {
    // Scan text until whitespace is finished.
    for ( ;; ) {
        // Check if it is a pure whitespace first.
        if ( IsWhitespace( lexer->position[0] ) ) {
            // Handle change of line
            if ( IsEndOfLine( lexer->position[0] ) )
                ++lexer->line;

            // Advance to next character
            ++lexer->position;

        } // Check for single line comments ("//")
        else if ( (lexer->position[0] == '/') && (lexer->position[1] == '/') ) {
            lexer->position += 2;
            while ( lexer->position[0] && !IsEndOfLine( lexer->position[0] ) ) {
                ++lexer->position;
            }
        } // Check for c-style comments
        else if ( (lexer->position[0] == '/') && (lexer->position[1] == '*') ) {
            lexer->position += 2;

            // Advance until the string is closed. Remember to check if line is changed.
            while ( !((lexer->position[0] == '*') && (lexer->position[1] == '/')) ) {
                // Handle change of line
                if ( IsEndOfLine( lexer->position[0] ) )
                    ++lexer->line;

                // Advance to next character
                ++lexer->position;
            }

            if ( lexer->position[0] == '*' ) {
                lexer->position += 2;
            }
        }
        else {
            break;
        }
    }
}

inline bool equalToken( Lexer* lexer, Token& token, Token::Type expected_type ) {
    nextToken( lexer, token );
    return token.type == expected_type;
}

inline bool expectToken( Lexer* lexer, Token& token, Token::Type expected_type ) {
    if ( lexer->error )
        return true;

    nextToken( lexer, token );

    const bool error = token.type != expected_type;
    lexer->error = error;

    if ( error ) {
        // Save line of error
        lexer->error_line = lexer->line;
    }

    return !error;
}

//
// Parser
// Allocate the string in one contiguous - and add the string length as a prefix (like a run length encoding)
static const char*          s_primitive_types_names = "6int32 7uint32 6int16 7uint16 5int8 6uint8 6int64 7uint64 6float 7double 5bool";

void initParser( Parser* parser, Lexer* lexer, uint32_t max_types ) {

    parser->lexer = lexer;

    // Add primitive types
    parser->types_count = 11;
    parser->types_max = max_types;
    parser->types = new ast::Type[max_types];

    // Use a single string with run-length encoding of names.
    char* names = (char*)s_primitive_types_names;

    for ( uint32_t i = 0; i < parser->types_count; ++i ) {
        ast::Type& primitive_type = parser->types[i];

        primitive_type.type = ast::Type::Types_Primitive;
        // Get the length encoded as first character of the name and remove the space (length - 1)
        const uint32_t length = names[0] - '0';
        primitive_type.name.length = length - 1;
        // Skip first character and let this string point into the master one.
        primitive_type.name.text = ++names;
        primitive_type.primitive_type = (ast::Type::PrimitiveTypes)i;
        // Advance to next name
        names += length;
    }
}

void generateAST( Parser* parser ) {

    // Read source text until the end.
    // The main body can be a list of declarations.
    bool parsing = true;

    while ( parsing ) {

        Token token;
        nextToken( parser->lexer, token );

        switch ( token.type ) {

            case Token::Token_Identifier:
            {
                identifier( parser, token );
                break;
            }

            case Token::Type::Token_EndOfStream:
            {
                parsing = false;
                break;
            }
        }
    }
}

static bool expectKeyword( const StringRef& text, uint32_t length, const char* expected_keyword ) {
    if ( text.length != length )
        return false;

    return memcmp(text.text, expected_keyword, length) == 0;
}

inline void identifier( Parser* parser, const Token& token ) {

    // Scan the name to know which 
    for ( uint32_t i = 0; i < token.text.length; ++i ) {
        char c = *(token.text.text + i);

        switch ( c ) {
            case 's':
            {
                if ( expectKeyword( token.text, 6, "struct" ) ) {
                    declarationStruct( parser );
                    return;
                }
                    
                break;
            }

            case 'e':
            {
                if ( expectKeyword( token.text, 4, "enum" ) ) {
                    declarationEnum( parser );
                    return;
                }
                break;
            }

            case 'c':
            {
                if ( expectKeyword( token.text, 7, "command" ) ) {
                    declarationCommand( parser );
                    return;
                }
                break;
            }
        }
    }
}

inline const ast::Type* findType( Parser* parser, const StringRef& name ) {

    for ( uint32_t i = 0; i < parser->types_count; ++i ) {
        const ast::Type* type = &parser->types[i];
        if ( equals( name, type->name ) ) {
            return type;
        }
    }
    return nullptr;
}

inline void declarationStruct( Parser* parser ) {
    // name
    Token token;
    if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    // Cache name string
    StringRef name = token.text;
    
    if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    // Add new type
    ast::Type& type = parser->types[parser->types_count++];
    type.name = name;
    type.type = ast::Type::Types_Struct;
    type.exportable = true;

    // Parse struct internals
    while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {
            declarationVariable( parser, token.text, type );
        }
    }
}

inline void declarationVariable( Parser* parser, const StringRef& type_name, ast::Type& type ) {
    const ast::Type* variable_type = findType( parser, type_name );
    Token token;
    // Name
    if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    // Cache name string
    StringRef name = token.text;

    if ( !expectToken( parser->lexer, token, Token::Token_Semicolon ) ) {
        return;
    }

    type.types.emplace_back( variable_type );
    type.names.emplace_back( name );
}

inline void declarationEnum( Parser* parser ) {
    Token token;
    // Name
    if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    // Cache name string
    StringRef name = token.text;

    // Optional ': type' for the enum
    nextToken( parser->lexer, token );
    if ( token.type == Token::Token_Colon ) {
        // Skip to open brace
        nextToken( parser->lexer, token );
        // Token now contains type_name
        nextToken( parser->lexer, token );
        // Token now contains open brace.
    }
    
    if ( token.type != Token::Token_OpenBrace ) {
        return;
    }

    // Add new type
    ast::Type& type = parser->types[parser->types_count++];
    type.name = name;
    type.type = ast::Type::Types_Enum;
    type.exportable = true;

    // Parse struct internals
    while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {
            type.names.emplace_back( token.text );
        }
    }
}

inline void declarationCommand( Parser* parser ) {
    // name
    Token token;
    if ( !expectToken( parser->lexer, token, Token::Token_Identifier ) ) {
        return;
    }

    // Cache name string
    StringRef name = token.text;

    if ( !expectToken( parser->lexer, token, Token::Token_OpenBrace ) ) {
        return;
    }

    // Add new type
    ast::Type& command_type = parser->types[parser->types_count++];
    command_type.name = name;
    command_type.type = ast::Type::Types_Command;
    command_type.exportable = true;

    // Parse struct internals
    while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {

        if ( token.type == Token::Token_Identifier ) {
            // Create a new type for each command
            // Add new type
            ast::Type& type = parser->types[parser->types_count++];
            type.name = token.text;
            type.type = ast::Type::Types_Struct;
            type.exportable = false;

            while ( !equalToken( parser->lexer, token, Token::Token_CloseBrace ) ) {
                if ( token.type == Token::Token_Identifier ) {
                    declarationVariable( parser, token.text, type );
                }
            }

            command_type.names.emplace_back( type.name );
            command_type.types.emplace_back( &type );
        }
    }
}

//
// CodeGenerator methods
//

static const char* s_primitive_type_cpp[] = { "int32_t", "uint32_t", "int16_t", "uint16_t", "int8_t", "uint8_t", "int64_t", "uint64_t", "float", "double", "bool" };
static const char* s_primitive_type_imgui[] = { "ImGuiDataType_S32", "ImGuiDataType_U32", "ImGuiDataType_S16", "ImGuiDataType_U16", "ImGuiDataType_S8", "ImGuiDataType_U8", "ImGuiDataType_S64", "ImGuiDataType_U64", "ImGuiDataType_Float", "ImGuiDataType_Double" };

inline void initCodeGenerator( CodeGenerator* code_generator, const Parser* parser, uint32_t buffer_size ) {
    code_generator->parser = parser;

    code_generator->string_buffer_0.init( buffer_size );
    code_generator->string_buffer_1.init( buffer_size );
    code_generator->string_buffer_2.init( buffer_size );
}

void generateCode( CodeGenerator* code_generator, const char* filename ) {

    // Create file
    FILE* output_file;
    fopen_s( &output_file, filename, "w" );

    if ( !output_file ) {
        printf( "Error opening file. Aborting. \n" );
        return;
    }

    fprintf( output_file, "\n#include <stdint.h>\n\n// This file is autogenerated!\n" );

    // Output all the types needed
    const Parser& parser = *code_generator->parser;
    for ( uint32_t i = 0; i < parser.types_count; ++i ) {
        const ast::Type& type = parser.types[i];

        if ( !type.exportable )
            continue;

        switch ( type.type ) {
            case ast::Type::Types_Struct:
            {
                outputCPPStruct( code_generator, output_file, type );
                break;
            }

            case ast::Type::Types_Enum:
            {
                outputCPPEnum( code_generator, output_file, type );
                break;
            }

            case ast::Type::Types_Command:
            {
                outputCPPCommand( code_generator, output_file, type );
                break;
            }
        }
    }

    fclose( output_file );
}

void outputCPPStruct( CodeGenerator* code_generator, FILE* output, const ast::Type& type ) {
    const char* tabs = "";

    code_generator->string_buffer_0.clear();
    code_generator->string_buffer_1.clear();
    code_generator->string_buffer_2.clear();

    StringBuffer& ui_code = code_generator->string_buffer_0;

    char name_buffer[256], member_name_buffer[256], member_type_buffer[256];
    copy( type.name, name_buffer, 256 );

    if ( code_generator->generate_imgui ) {
        ui_code.append( "\n\tvoid reflectMembers() {\n" );
    }

    fprintf( output, "%sstruct %s {\n\n", tabs, name_buffer );

    for ( int i = 0; i < type.types.size(); ++i ) {
        const ast::Type& member_type = *type.types[i];
        const StringRef& member_name = type.names[i];

        copy( member_name, member_name_buffer, 256 );

        // Translate type name based on output language.
        switch ( member_type.type ) {
            case ast::Type::Types_Primitive:
            {
                strcpy_s( member_type_buffer, 256, s_primitive_type_cpp[member_type.primitive_type] );
                fprintf( output, "%s\t%s %s;\n", tabs, member_type_buffer, member_name_buffer );
                
                if ( code_generator->generate_imgui ) {
                    switch ( member_type.primitive_type ) {
                        case ast::Type::Primitive_Int8:
                        case ast::Type::Primitive_Uint8:
                        case ast::Type::Primitive_Int16:
                        case ast::Type::Primitive_Uint16:
                        case ast::Type::Primitive_Int32:
                        case ast::Type::Primitive_Uint32:
                        case ast::Type::Primitive_Int64:
                        case ast::Type::Primitive_Uint64:
                        case ast::Type::Primitive_Float:
                        case ast::Type::Primitive_Double:
                        {
                            ui_code.append( "\t\tImGui::InputScalar( \"%s\", %s, &%s );\n", member_name_buffer, s_primitive_type_imgui[member_type.primitive_type], member_name_buffer );
                            
                            break;
                        }
                        
                        case ast::Type::Primitive_Bool:
                        {
                            ui_code.append( "\t\tImGui::Checkbox( \"%s\", &%s );\n", member_name_buffer, member_name_buffer );
                            break;
                        }
                    }
                }

                break;
            }

            case ast::Type::Types_Struct:
            {
                copy( member_type.name, member_type_buffer, 256 );
                fprintf( output, "%s\t%s %s;\n", tabs, member_type_buffer, member_name_buffer );

                if ( code_generator->generate_imgui ) {
                    ui_code.append( "\t\tImGui::Text(\"%s\");\n", member_name_buffer );
                    ui_code.append( "\t\t%s.reflectMembers();\n", member_name_buffer );
                }

                break;
            }

            case ast::Type::Types_Enum:
            {
                copy( member_type.name, member_type_buffer, 256 );
                fprintf( output, "%s\t%s::Enum %s;\n", tabs, member_type_buffer, member_name_buffer );

                if ( code_generator->generate_imgui ) {
                    ui_code.append( "\t\tImGui::Combo( \"%s\", (int32_t*)&%s, %s::s_value_names, %s::Count );\n", member_name_buffer, member_name_buffer, member_type_buffer, member_type_buffer );
                }

                break;
            }

            default:
            {
                break;
            }
        }
    }

    ui_code.append( "\t}" );
    ui_code.append( "\n\n\tvoid reflectUI() {\n\t\tImGui::Begin(\"%s\");\n\t\treflectMembers();\n\t\tImGui::End();\n\t}\n", name_buffer );

    fprintf( output, "%s\n", ui_code.data );

    fprintf( output, "\n%s}; // struct %s\n\n", tabs, name_buffer );
}

void outputCPPEnum( CodeGenerator* code_generator, FILE* output, const ast::Type& type ) {

    // Empty enum: skip output.
    if ( type.names.size() == 0 )
        return;

    code_generator->string_buffer_0.clear();
    code_generator->string_buffer_1.clear();
    code_generator->string_buffer_2.clear();

    StringBuffer& values = code_generator->string_buffer_0;
    StringBuffer& value_names = code_generator->string_buffer_1;
    StringBuffer& value_masks = code_generator->string_buffer_2;

    value_names.append( "\"" );

    bool add_max = true;
    bool add_mask = true;

    char name_buffer[256];

    // Enums with more than 1 values
    if ( type.names.size() > 1 ) {
        const uint32_t max_values = type.names.size() - 1;
        for ( uint32_t v = 0; v < max_values; ++v ) {

            if ( add_mask ) {
                value_masks.append( type.names[v] );
                value_masks.append( "_mask = 1 << " );
                value_masks.append( _itoa( v, name_buffer, 10 ) );
                value_masks.append( ", " );
            }

            values.append( type.names[v] );
            values.append( ", " );

            value_names.append( type.names[v] );
            value_names.append( "\", \"" );
        }

        if ( add_mask ) {
            value_masks.append( type.names[max_values] );
            value_masks.append( "_mask = 1 << " );
            value_masks.append( _itoa( max_values, name_buffer, 10 ) );
        }

        values.append( type.names[max_values] );

        value_names.append( type.names[max_values] );
        value_names.append( "\"" );
    }
    else {
        
        if ( add_mask ) {
            value_masks.append( type.names[0] );
            value_masks.append( "_mask = 1 << " );
            value_masks.append( _itoa( 0, name_buffer, 10 ) );
        }

        values.append( type.names[0] );

        value_names.append( type.names[0] );
        value_names.append( "\"" );
    }

    if ( add_max ) {
        values.append( ", Count" );

        value_names.append( ", \"Count\"" );

        if ( add_mask ) {
            value_masks.append( ", Count_mask = 1 << " );
            value_masks.append( _itoa( type.names.size(), name_buffer, 10 ) );
        }
    }
    
    copy( type.name, name_buffer, 256 );

    fprintf( output, "namespace %s {\n", name_buffer );

    fprintf( output, "\tenum Enum {\n" );
    fprintf( output, "\t\t%s\n", values.data );
    fprintf( output, "\t};\n" );

    // Write the mask
    if ( add_mask ) {
        fprintf( output, "\n\tenum Mask {\n" );
        fprintf( output, "\t\t%s\n", value_masks.data );
        fprintf( output, "\t};\n" );
    }

    // Write the string values
    fprintf( output, "\n\tstatic const char* s_value_names[] = {\n" );
    fprintf( output, "\t\t%s\n", value_names.data );
    fprintf( output, "\t};\n" );

    fprintf( output, "\n\tstatic const char* ToString( Enum e ) {\n" );
    fprintf( output, "\t\treturn s_value_names[(int)e];\n" );
    fprintf( output, "\t}\n" );

    fprintf( output, "} // namespace %s\n\n", name_buffer );
}

void outputCPPCommand( CodeGenerator* code_generator, FILE* output, const ast::Type& type ) {

    char name_buffer[256], member_name_buffer[256], member_type_buffer[256];
    copy( type.name, name_buffer, 256 );

    fprintf( output, "namespace %s {\n", name_buffer );

    // Add enum with all types
    fprintf( output, "\tenum Type {\n" );
    fprintf( output, "\t\t" );
    for ( int i = 0; i < type.types.size() - 1; ++i ) {
        const ast::Type& command_type = *type.types[i];
        copy( command_type.name, name_buffer, 256 );
        fprintf( output, "Type_%s, ", name_buffer );
    }

    const ast::Type* last_type = type.types[type.types.size() - 1];
    copy( last_type->name, name_buffer, 256 );
    fprintf( output, "Type_%s", name_buffer );
    fprintf( output, "\n\t};\n\n" );

    const char* tabs = "\t";

    for ( int i = 0; i < type.types.size(); ++i ) {
        const ast::Type& command_type = *type.types[i];

        copy( command_type.name, member_type_buffer, 256 );
        fprintf( output, "%sstruct %s {\n\n", tabs, member_type_buffer );
        
        for ( int i = 0; i < command_type.types.size(); ++i ) {
            const ast::Type& member_type = *command_type.types[i];
            const StringRef& member_name = command_type.names[i];

            copy( member_name, member_name_buffer, 256 );

            // Translate type name based on output language.
            switch ( member_type.type ) {
                case ast::Type::Types_Primitive:
                {
                    strcpy_s( member_type_buffer, 256, s_primitive_type_cpp[member_type.primitive_type] );
                    fprintf( output, "%s\t%s %s;\n", tabs, member_type_buffer, member_name_buffer );


                    break;
                }

                case ast::Type::Types_Struct:
                {
                    copy( member_type.name, member_type_buffer, 256 );
                    fprintf( output, "%s\t%s %s;\n", tabs, member_type_buffer, member_name_buffer );

                    break;
                }

                case ast::Type::Types_Enum:
                {
                    copy( member_type.name, member_type_buffer, 256 );
                    fprintf( output, "%s\t%s::Enum %s;\n", tabs, member_type_buffer, member_name_buffer );

                    break;
                }

                default:
                {
                    break;
                }
            }
        }

        copy( command_type.name, member_type_buffer, 256 );

        fprintf( output, "\n%s\tstatic Type GetType() { return Type_%s; }\n", tabs, member_type_buffer );
        fprintf( output, "\n%s}; // struct %s\n\n", tabs, name_buffer );
    }

    copy( type.name, name_buffer, 256 );
    fprintf( output, "}; // namespace %s\n\n", name_buffer );

}