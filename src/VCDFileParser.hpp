/*!
@file
@brief Contains the declaration of the parser driver class.
*/

#ifndef VCD_PARSER_DRIVER_HPP
#define VCD_PARSER_DRIVER_HPP

#include <string>
#include <map>
#include <set>
#include <stack>
#include <limits>

#include "VCDParser.hpp"
#include "VCDTypes.hpp"
#include "VCDFile.hpp"

// Forward declaration for reentrant scanner
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#define YY_DECL \
    VCDParser::parser::symbol_type yylex (yyscan_t yyscanner)

YY_DECL;


/*!
@brief Class for parsing files containing CSP notation.
*/
class VCDFileParser {

    public:

        //! Create a new parser/
        VCDFileParser();
        virtual ~VCDFileParser();

        /*!
        @brief Parse the suppled file.
        @returns A handle to the parsed VCDFile object or nullptr if parsing
        fails.
        */
        VCDFile * parse_file(const std::string & filepath);

        //! The current file being parsed.
        std::string filepath;

        //! Should we debug tokenising?
        bool trace_scanning;

        //! Should we debug parsing of tokens?
        bool trace_parsing;

        //! Ignore anything before this timepoint
        VCDTime start_time;

        //! Ignore anything after this timepoint
        VCDTime end_time;

        //! Reports errors to stderr.
        void error(const VCDParser::location & l, const std::string & m);

        //! Reports errors to stderr.
        void error(const std::string & m);

        //! Current file being parsed and constructed.
        VCDFile * fh;

        //! Current stack of scopes being parsed.
        std::stack<VCDScope*> scopes;

        //! Current time while parsing the VCD file (moved from global).
        VCDTime current_time;

        //! Location tracker for lexer (moved from static).
        VCDParser::location loc;

        //! Wrapper for calling reentrant yylex
        VCDParser::parser::symbol_type get_next_token();

    protected:

        //! Reentrant scanner state
        yyscan_t scanner;

        //! File handle for input
        FILE* input_file;

        //! Utility function for starting parsing.
        void scan_begin ();

        //! Utility function for stopping parsing.
        void scan_end   ();
};

#endif

