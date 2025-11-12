/*!
@file
@brief Definition of the VCDFileParser class
*/

#include "VCDFileParser.hpp"
#include <cerrno>
#include <cstring>

// Forward declarations for flex reentrant functions
extern "C" {
    int yylex_init(yyscan_t* scanner);
    int yylex_destroy(yyscan_t scanner);
    void yyset_in(FILE* in_str, yyscan_t yyscanner);
    void yyset_extra(void* user_defined, yyscan_t yyscanner);
    void yyset_debug(int debug_flag, yyscan_t yyscanner);
}

VCDFileParser::VCDFileParser() {

    this -> start_time = -std::numeric_limits<decltype(start_time)>::max();
    this -> end_time = std::numeric_limits<decltype(end_time)>::max() ;
    this -> current_time = 0;

    this->trace_scanning = false;
    this->trace_parsing = false;

    this->scanner = nullptr;
    this->input_file = nullptr;
}

VCDFileParser::~VCDFileParser()
{
    // Scanner cleanup is handled in scan_end()
}

VCDFile *VCDFileParser::parse_file(const std::string &filepath)
{

    this->filepath = filepath;
    this->current_time = 0;  // Reset current time for each parse

    scan_begin();

    this->fh = new VCDFile();
    VCDFile *tr = this->fh;

    this->fh->root_scope = new VCDScope;
    this->fh->root_scope->name = std::string("$root");
    this->fh->root_scope->type = VCD_SCOPE_ROOT;

    this->scopes.push(this->fh->root_scope);

    this -> fh -> root_scope = new VCDScope;
    this -> fh -> root_scope -> name = std::string("");
    this -> fh -> root_scope -> type = VCD_SCOPE_ROOT;

    this -> scopes.push(this -> fh -> root_scope);

    tr -> add_scope(scopes.top());

    VCDParser::parser parser(*this);

    parser.set_debug_level(trace_parsing);

    int result = parser.parse();

    scopes.pop();

    scan_end();

    if (result == 0)
    {
        this->fh = nullptr;
        return tr;
    }
    else
    {
        tr = nullptr;
        delete this->fh;
        return nullptr;
    }
}

void VCDFileParser::error(const VCDParser::location &l, const std::string &m)
{
    std::cerr << "line " << l.begin.line
              << std::endl;
    std::cerr << " : " << m << std::endl;
}

void VCDFileParser::error(const std::string & m){
    std::cerr << " : "<<m<<std::endl;
}

VCDParser::parser::symbol_type VCDFileParser::get_next_token() {
    return yylex(scanner);
}

void VCDFileParser::scan_begin() {
    // Initialize the reentrant scanner
    yylex_init(&scanner);

    // Set the scanner extra data to point to this parser instance
    yyset_extra(this, scanner);

    // Set debug flag
    yyset_debug(trace_scanning ? 1 : 0, scanner);

    // Open the input file
    if(filepath.empty() || filepath == "-") {
        input_file = stdin;
    } else {
        input_file = fopen(filepath.c_str(), "r");
        if(!input_file) {
            error("Cannot open " + filepath + ": " + strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    // Set the input file for the scanner
    yyset_in(input_file, scanner);
}

void VCDFileParser::scan_end() {
    // Close the input file if it's not stdin
    if(input_file && input_file != stdin) {
        fclose(input_file);
    }
    input_file = nullptr;

    // Destroy the scanner
    if(scanner) {
        yylex_destroy(scanner);
        scanner = nullptr;
    }
}

#ifdef VCD_PARSER_STANDALONE

void print_scope_signals(VCDScope * scope)
{
    for(VCDSignal * signal : scope -> signals) {

        std::cout << "\t" << signal -> hash << "\t" 
                    << signal -> reference;

        if(signal -> size > 1) {
            std::cout << "[" << signal -> lindex << ":" << signal -> rindex << "]";
        } else if (signal -> lindex >= 0) {
            std::cout << "[" << signal -> lindex << "]";
        }
        
        std::cout << std::endl;

    }
}

void traverse_scope(std::string parent, VCDScope * scope)
{
    std::string local_parent = parent;

    if (parent.length())
        local_parent += ".";
    local_parent += scope->name;
    std::cout << "Scope: " << local_parent  << std::endl;
    print_scope_signals(scope);
    for (auto child : scope->children) {
        std::cout << "Child:\n";
        traverse_scope(local_parent, child);
    }
}
/*!
@brief Standalone test function to allow testing of the VCD file parser.
*/
int main (int argc, char** argv){

    std::string infile (argv[1]);

    std::cout << "Parsing " << infile << std::endl;

    VCDFileParser parser;

    VCDFile * trace = parser.parse_file(infile);

    if(trace) {
        std::cout << "Parse successful." << std::endl;
        std::cout << "Version:       " << trace -> version << std::endl;
        std::cout << "Date:          " << trace -> date << std::endl;
        std::cout << "Signal count:  " << trace -> get_signals() -> size() <<std::endl;
        std::cout << "Times Recorded:" << trace -> get_timestamps() -> size() << std::endl;
    
        // Print out every signal in every scope.
        for(VCDScope * scope : *trace -> get_scopes()) {
            if (scope->parent)
                continue;
            traverse_scope(std::string(""), scope);
        }

        delete trace;
        
        return 0;
    } else {
        std::cout << "Parse Failed." << std::endl;
        return 1;
    }

}

#endif
