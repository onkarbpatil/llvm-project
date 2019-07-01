#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/SymbolTableListTraits.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm//IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include <system_error>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <stack>
#include <list>
//#include <cxxabi>

using namespace llvm;

struct access_pattern{
	std::string scope;
	std::string access="";           /*Usage pattern*/
};


struct data_properties{
	std::string name;		/*The first definition name*/
	std::string scope;			/*Original scope*/
	std::string d_scope;
	std::string func;
        std::string file;
	std::string size;
	std::string first_def="";		/*First def*/
        int decl_line;
        int decl_col;
        int start_line;
        int start_column;
        int end_line;
        int end_column;
	bool decl_flag = true;
	bool init_flag = true;
	char alloc_type = 's';
	std::string cur_pointer;		/*Current data structure pointed*/
	std::map<std::string, std::string> alias;	/*function, name*/
	std::map<std::string, std::map<std::string, std::list<std::string>>> pointer_alias;	/*function, scope, name*/
	std::map<std::string, std::string> cur_def; /*current func, Current def*/
	std::map<std::string, std::list<std::string>> prev_def;		/*prev defs*/
	std::map<std::string, std::map<std::string, std::string>> usage;	/*func, scope, access_patterns*/
	std::map<std::string, std::map<std::string, std::string>> alloc;	/*func, scope, Decision*/
	std::map<std::string, std::map<std::string, int>> acc_pat;	/*func, scope, Access Pattern*/
	std::map<std::string, std::map<std::string, std::string>> access_pattern;	/*func, scope, Access Pattern*/
	std::map<std::string, std::map<std::string, std::list<struct data_properties *>>> expr_prox;	/*func, scope, data properties*/
	std::map<std::string, std::map<std::string, std::list<struct data_properties *>>> index_var;    /*func, scope, data properties*/
	std::map<std::string, std::map<std::string, std::list<struct data_properties *>>> scope_prox;	/*func, scope, data properties*/
	std::list<struct memory_region> alloc_info;
};

struct interim{
	std::string var;
	int acc_pat = 0;
	std::list<struct data_properties *>dplist;
};

struct scope_info{
	std::string name;
	std::string file;
	std::string func;
	int start_line;
        int start_column;
        int end_line;
        int end_column;
	int abs_nesting_score;
	int type_nesting_score;
};

struct memory_region{
	std::string alloc_func;
	std::string alloc_scope;
	std::string alloc_file;
	std::string free_func;
	std::string free_scope;
	std::string free_file;
	int array_ref = 0;
	int alloc_line = -1;
	int alloc_col = -1;
	int free_line = -1;
	int free_col = -1;
};

struct numa_nodes{
        int numa_id;
	std::string mem_type;
	long double avg_bw;
	long double read_bw;
	long double write_bw;
	long double rw_bw;
	long double ran_bw;
	long double lin_bw;
	long double same_bw;
	long double diff_bw;
	long double avg_lat;
        long double read_lat;
        long double write_lat;
        long double rw_lat;
        long double ran_lat;
        long double lin_lat;
        long double same_lat;
        long double diff_lat;
	
	
};


extern std::map<std::string, std::ifstream *> func_map;
extern std::list<struct data_properties> data_structures;
extern std::list<std::string> call_parameters;
extern std::list<struct scope_info> scope_list;
extern std::stack<std::string> cur_func;
extern std::stack<std::string> cur_scope;
extern std::stack<std::string> nest_scope;
extern std::stack<int> type_scores;
extern std::map<std::string, int>scope_limit;
extern std::map<std::string, int>op_val;
extern std::string cur_line;
extern std::string prev_func;
extern std::string malloc_mem_name;
extern std::list<struct numa_nodes> numa_list;
extern std::list<struct interim> temp_vars;
extern char * cur_word;
extern bool iflag;
extern int scope_base;
extern int for_scope_no;
extern int if_scope_no;
extern int while_scope_no;
extern int mr_cnt;
extern std::list<int> dis_lines;
extern std::stack<int> prev_scope_base;
extern bool cflag;
extern bool mflag;
extern double same_expr_prox;
extern double diff_expr_prox;
extern double rw_access_score;
extern double ro_access_score;
extern double wo_access_score;
extern double lin_patt_score;
extern double seq_patt_score;
extern double ran_patt_score;

extern struct data_properties * find_data(std::string name);
extern bool check_func(std::string func_name);
extern void get_nesting();
extern void get_numa_info();
extern struct data_properties * find_data(std::string name);
extern void next_line(std::string fname);
extern void next_word();
extern void memoryDef();
extern void memoryUse();
extern void arrays();
extern void parameter_map();
extern void forEnd();
extern void whileEnd();
extern void ifEnd();
extern void func_call();
extern void entryVars();
extern void analyze_line(char * str);
extern void control_flow_traversal();
extern void make_decision();
extern void file_cleanup();
