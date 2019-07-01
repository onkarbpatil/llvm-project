//===- IDP.cpp - Intelligent Data Placement ---------------===//
//
//
//
//===----------------------------------------------------------------------===//
//
// This file implements the Intelligent Data Pass.
//
//===----------------------------------------------------------------------===//

#include "IDP.h"

using namespace llvm;

#define DEBUG_TYPE "hello"
/*//STATISTIC(HelloCounter, "Counts number of functions greeted");

*/

std::map<std::string, std::ifstream *> func_map;
std::list<struct data_properties> data_structures;
std::list<std::string> call_parameters;
std::list<struct scope_info> scope_list;
std::stack<std::string> cur_scope;
std::stack<std::string> nest_scope;
std::stack<std::string> cur_func;
std::stack<int> type_scores;
std::map<std::string, int>scope_limit;
std::map<std::string, int>op_val;
std::list<struct interim> temp_vars;
std::string cur_line;
std::string cpy_line;
std::string prev_func;
std::string malloc_mem_name;
std::list<struct numa_nodes> numa_list;
char * cur_word;
int scope_base = -1;
int mr_cnt = 0;
int for_scope_no = 0;
int if_scope_no = 0;
int while_scope_no = 0;
int if_stack_bal = 0;
double same_expr_prox;
double diff_expr_prox;
double rw_access_score;
double ro_access_score;
double wo_access_score;
double lin_patt_score;
double seq_patt_score;
double ran_patt_score;
std::list<int> dis_lines;
std::stack<int> prev_scope_base;
bool iflag = false;
bool cflag = false;
bool mflag = false;
bool inc_flag = false;
bool ar_flag = false;

namespace {


void assign_op_val(){
	op_val["add"] = 1;
	op_val["sub"] = 1;
	op_val["inc"] = 1;
        op_val["dec"] = 1;
	op_val["rem"] = 0;
        op_val["mul"] = 1000;
	op_val["div"] = 1000;
}

void get_scope(struct data_properties * dt){
	std::string name;
	std::string scope;
	std::string func;
	int decl_line;
	int decl_col;
	std::string file_name;
	int start_line;
	int start_column;
	int end_line;
	int end_column;

	std::ifstream decl_file;
	decl_file.open("vardeclscope");
	while(decl_file>>name>>file_name>>func>>scope>>decl_line>>decl_col>>start_line>>start_column>>end_line>>end_column){
		if(((name.compare(dt->name)==0)&&(func.compare(dt->first_def)==0))||((name.compare(dt->name)==0)&&(scope.compare("global")==0))){
			(dt)->d_scope = scope;
			(dt)->file = file_name;
			(dt)->decl_line = decl_line;
			(dt)->decl_col = decl_col;
			(dt)->func = func;
			(dt)->start_line = start_line;
			(dt)->start_column = start_column;
			(dt)->end_line = end_line;
			(dt)->end_column = end_column;
			break;
		}
	}
	decl_file.close();

}

void get_alias(struct data_properties * dt,  int ad, int ar){
	std::string alias = std::string(dt->name);
        std::string name;
	int array_ref;
        std::string alloc_scope;
        std::string alloc_func;
        int alloc_line;
        int alloc_col;
        std::string alloc_file;
        int free_line;
        int free_col;
        std::string free_func;
        std::string free_scope;
	std::string free_file;
	

        std::ifstream alias_file;
	struct memory_region s;
        alias_file.open("pointeralias");
        while(alias_file>>name>>array_ref>>alloc_file>>alloc_func>>alloc_scope>>alloc_line>>alloc_col>>free_file>>free_func>>free_scope>>free_line>>free_col){
std::cout<<"get_alias\n";
fflush(NULL);
		if(ad == 0){
                	if(((name.compare(alias)==0)&&(alloc_func.compare(cur_func.top())==0)&&(array_ref==ar)&&(alloc_scope.compare(cur_scope.top())||alloc_scope.compare("global")))){
std::cout<<"malloc_alias\n";
fflush(NULL);
                        	s.array_ref = array_ref;
                        	s.alloc_file = alloc_file;
                        	s.alloc_col = alloc_col;
                        	s.alloc_line = alloc_line;
                        	s.alloc_scope = alloc_scope;
                        	s.alloc_func = alloc_func;
                        	s.free_file = free_file;
                        	s.free_line = free_line;
                        	s.free_col = free_col;
                        	s.free_func = free_func;
                        	s.free_scope = free_scope;
                        	break;
                	}
		}
		if(ad == 1){
std::cout<<"a_ref "<<array_ref<<" "<<ar<<"\n";
fflush(NULL);
			if((name.compare(alias)==0)&&(free_func.compare(cur_func.top())==0)&&(array_ref == ar)&&(free_scope.compare(cur_scope.top())||free_scope.compare("global"))){
std::cout<<"free_alias\n";
fflush(NULL);
                        	s.array_ref = array_ref;
                       		s.alloc_file = alloc_file;
                       		s.alloc_line = alloc_line;
                        	s.alloc_scope = alloc_scope;
                       		s.alloc_col = alloc_col;
                       		s.alloc_func = alloc_func;
                        	s.free_file = free_file;
                       		s.free_line = free_line;
                       		s.free_col = free_col;
                       		s.free_func = free_func;
                       		s.free_scope = free_scope;
                       		break;
			}
		}
        }
	dt->alloc_info.push_back(s);
        alias_file.close();
}

bool check_func(std::string func_name){
        std::string name;
        std::string file_name;
        std::string func;
        int start_line;
        int start_column;
        int end_line;
        int end_column;
        int nesting_score;
        struct scope_info si;
        std::ifstream nest_file;
        nest_file.open("nestingscore");
std::cout<<"Check Func\n";
        while(nest_file>>name>>file_name>>func>>start_line>>start_column>>end_line>>end_column>>nesting_score){
		if(func_name.compare(func)==0)
			return true;
	}
        nest_file.close();
	return false;
}


void get_nesting(){
	std::string name;
	std::string file_name;
	std::string func;
	int start_line;
	int start_column;
	int end_line;
	int end_column;
	int nesting_score;
	int line_no = 0;
	struct scope_info si;
	std::ifstream nest_file;
	nest_file.open("nestingscore");
std::cout<<"Nesting\n";
	while(nest_file>>name>>file_name>>func>>start_line>>start_column>>end_line>>end_column>>nesting_score){
std::cout<<"Nest: "<<(nest_scope.size()-scope_base)<<" "<<nesting_score<<"\n";
		line_no++;
		if((nest_scope.top().compare("entry:")==0)&&(func.compare(cur_func.top())==0)&&(((int)nest_scope.size() - scope_base) == nesting_score)){	
			si.name = cur_func.top();
			si.file = file_name;
			si.func = func;
			si.start_line = start_line;
			si.start_column = start_column;
			si.end_line = end_line;
			si.end_column = end_column;
			si.abs_nesting_score = nesting_score;
			si.type_nesting_score = 0;
			auto it = std::find(dis_lines.begin(), dis_lines.end(), line_no);
			if(it == dis_lines.end()){
				dis_lines.push_back(line_no);
				type_scores.push(-1);
				type_scores.push(for_scope_no);
				type_scores.push(if_scope_no);
				type_scores.push(while_scope_no);
				for_scope_no = 0;
				if_scope_no = 0;
				while_scope_no = 0;
				scope_list.push_back(si);
				break;
			}
			else{
				continue;
			}	
		}
		else if(((((nest_scope.top()).find("if.then")!=std::string::npos)&&(name.compare("if") == 0))||(((nest_scope.top()).find("if.else")!=std::string::npos)&&((name.compare("elseif") == 0)||(name.compare("else") == 0))))&&(func.compare(cur_func.top())==0)&&(((int)nest_scope.size() - scope_base - if_stack_bal) == nesting_score)){
//Need to handle the nesting scope stack for IF statements
			si.name = cur_scope.top();
                        si.file = file_name;
                        si.func = func;
                        si.start_line = start_line;
                        si.start_column = start_column;
                        si.end_line = end_line;
                        si.end_column = end_column;
                        si.abs_nesting_score = nesting_score;
                        auto it = std::find(dis_lines.begin(), dis_lines.end(), line_no);
                        if(it == dis_lines.end()){
                                dis_lines.push_back(line_no);
				if(name.find("else") == std::string::npos)
                                	if_scope_no++;
                                si.type_nesting_score = if_scope_no;
				scope_list.push_back(si);
                                break;
                        }
                        else{
                                continue;
                        }
		}
		else if(((nest_scope.top()).find(name)!=std::string::npos)&&(func.compare(cur_func.top())==0)&&(((int)nest_scope.size() - scope_base) == nesting_score)){
			si.name = cur_scope.top();
			si.file = file_name;
			si.func = func;
			si.start_line = start_line;
			si.start_column = start_column;
			si.end_line = end_line;
			si.end_column = end_column;
			si.abs_nesting_score = nesting_score;
			auto it = std::find(dis_lines.begin(), dis_lines.end(), line_no);
			if(it == dis_lines.end()){
				dis_lines.push_back(line_no);
				if(name.find("for")!=std::string::npos){
					for_scope_no++;
					si.type_nesting_score = for_scope_no;
				}
				if(name.find("while")!=std::string::npos){
                        	        while_scope_no++;
                        	        si.type_nesting_score = while_scope_no;
                        	}
				scope_list.push_back(si);
				break;
			}
			else{
				continue;
			}	
		}

	}
	nest_file.close();
}

void get_numa_info(){
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

	struct numa_nodes temp_node;

	long double max_read_bw=0.0;
        long double min_write_bw=9999999.999999;
        long double max_rw_bw=0.0;
        long double min_rand_bw=9999999.999999;
        long double max_lin_bw=0.0;
        long double max_same_bw=0.0;
        long double min_diff_bw=0.0;

	std::ifstream numa_file;
        numa_file.open("sicm_numa_config");
	while(numa_file>>numa_id>>mem_type>>avg_bw>>read_bw>>write_bw>>rw_bw>>ran_bw>>lin_bw>>same_bw>>diff_bw>>avg_lat>>read_lat>>write_lat>>rw_lat>>ran_lat>>lin_lat>>same_lat>>diff_lat){
		temp_node.numa_id = numa_id;
        	temp_node.mem_type = mem_type;
        	temp_node.avg_bw = avg_bw;
        	temp_node.read_bw = read_bw;
        	temp_node.write_bw = write_bw;
        	temp_node.rw_bw = rw_bw;
        	temp_node.ran_bw = ran_bw;
        	temp_node.lin_bw = lin_bw;
        	temp_node.same_bw = same_bw;
        	temp_node.diff_bw = diff_bw;
        	temp_node.avg_lat = avg_lat;
        	temp_node.read_lat = read_lat;
        	temp_node.write_lat = write_lat;
        	temp_node.rw_lat = rw_lat;
        	temp_node.ran_lat = ran_lat;
        	temp_node.lin_lat = lin_lat;
        	temp_node.same_lat = same_lat;
        	temp_node.diff_lat = diff_lat;
		
		if(read_bw > max_read_bw){
			max_read_bw = read_bw;
		}
		if(lin_bw > max_lin_bw){
                        max_lin_bw = lin_bw;
                }
		if(rw_bw > max_rw_bw){
                        max_rw_bw = rw_bw;
                }
		if(same_bw > max_same_bw){
                        max_same_bw = same_bw;
                }
		if(diff_bw < min_diff_bw){
                        min_diff_bw = diff_bw;
                }
		if(write_bw < min_write_bw){
                        min_write_bw = write_bw;
                }
		if(ran_bw < min_rand_bw){
                        min_rand_bw = ran_bw;
                }
		numa_list.push_front(temp_node);
		
	}
	same_expr_prox = max_same_bw/min_diff_bw;
	diff_expr_prox = min_diff_bw/min_diff_bw;
	rw_access_score = max_rw_bw/min_write_bw;
	ro_access_score = max_read_bw/min_write_bw;
	wo_access_score = min_write_bw/min_write_bw;
	lin_patt_score = max_lin_bw/min_rand_bw;
	seq_patt_score = max_rw_bw/min_rand_bw;
	ran_patt_score = min_rand_bw/min_rand_bw;

	numa_file.close();
}

struct interim * find_temp_var(std::string name){			/*Find a temporary variable*/
	for(std::list<struct interim>::iterator it = temp_vars.begin(); it != temp_vars.end(); it++){
		if(name.compare((*it).var) == 0){
			struct interim * in = &(*it);
			return (in);
		}
	}
	return NULL;
}

void update_temp_var(std::string name, std::string new_name){                 /*Update a temporary variable*/
        for(std::list<struct interim>::iterator it = temp_vars.begin(); it != temp_vars.end(); it++){
                if(name.compare((*it).var) == 0){
			(*it).var = new_name;
			/*Update the access pattern here*/
                }
        }
}

void remove_temp_var(std::string name){                 /*Remove a temporary variable*/
        for(std::list<struct interim>::iterator it = temp_vars.begin(); it != temp_vars.end(); it++){
                if(name.compare((*it).var) == 0){
/*Add to expr_prox/acc_pat*/
			if(!(*it).dplist.empty()){
std::cout<<"Remove: "<<(*it).var<<"\n";
std::fflush(NULL);
				for(std::list<struct data_properties *>::iterator it1 = (*it).dplist.begin(); it1 != (*it).dplist.end(); it1++){
					for(std::list<struct data_properties *>::iterator it2 = (*it).dplist.begin(); it2 != (*it).dplist.end(); it2++){
						std::list<struct data_properties *>::iterator it3;
                                		it3 = std::find((*it1)->expr_prox[cur_func.top()][cur_scope.top()].begin(), (*it1)->expr_prox[cur_func.top()][cur_scope.top()].end(), (*it2));
                                		if((it3 == (*it1)->expr_prox[cur_func.top()][cur_scope.top()].end())&&((*it1) != (*it2))){
                        	        		((*it1)->expr_prox[cur_func.top()][cur_scope.top()]).push_front(*it2);
std::cout<<"TV: "<<(*it2)->name<<"\n";
std::fflush(NULL);
						}
                        		}
					//((*it1)->expr_prox[cur_func.top()][cur_scope.top()]).unique();
				}
			}
                        temp_vars.erase(it);
			break;
                }
        }
}

std::string assign_access_pattern(int score){
	if(score == 1){
		return "sequential";
	}
	if(score == 2){
                return "strided";
        }
	if((score > 2)&&(score < 1000)){
                return "linear";
        }
	if(score >= 1000){
                return "random";
        }
	return "random";
	
}

void assign_index_prox_for(std::string f_cond, std::string f_body, std::string f_inc){
std::cout<<"AIP\n";
std::fflush(NULL);
	for(std::list<struct data_properties>::iterator it = data_structures.begin(); it != data_structures.end(); it++) {
		if(!((*it).index_var[cur_func.top()][f_cond].empty())){
			for(std::list<struct data_properties *>::iterator it1 = (*it).index_var[cur_func.top()][f_cond].begin(); it1 != (*it).index_var[cur_func.top()][f_cond].end(); it1++) {
				for(std::list<struct data_properties>::iterator it2 = data_structures.begin(); it2 != data_structures.end(); it2++) {
					if((*it1)->name.compare((*it2).name) == 0){
std::cout<<"Assign1: "<<(*it1)->name<<"\n";
std::cout<<(*it2).name<<"\n";
std::fflush(NULL);
						(*it).acc_pat[cur_func.top()][f_cond] += (*it2).acc_pat[cur_func.top()][f_cond];
						(*it).access_pattern[cur_func.top()][f_cond] = assign_access_pattern((*it).acc_pat[cur_func.top()][f_cond]);
					}
				}
			}
		}
		if(!((*it).index_var[cur_func.top()][f_body].empty())){
                        for(std::list<struct data_properties *>::iterator it1 = (*it).index_var[cur_func.top()][f_body].begin(); it1 != (*it).index_var[cur_func.top()][f_body].end(); it1++) {
                                for(std::list<struct data_properties>::iterator it2 = data_structures.begin(); it2 != data_structures.end(); it2++) {
std::cout<<"Assign2: "<<(*it1)->name<<"\n";
std::cout<<(*it2).name<<"\n";
std::fflush(NULL);
                                        if((*it1)->name.compare((*it2).name) == 0){
                                                (*it).acc_pat[cur_func.top()][f_body] += (*it2).acc_pat[cur_func.top()][f_body];
						(*it).access_pattern[cur_func.top()][f_body] = assign_access_pattern((*it).acc_pat[cur_func.top()][f_body]);
                                        }
                                }
                        }
                }
		if(!((*it).index_var[cur_func.top()][f_inc].empty())){
                        for(std::list<struct data_properties *>::iterator it1 = (*it).index_var[cur_func.top()][f_inc].begin(); it1 != (*it).index_var[cur_func.top()][f_inc].end(); it1++) {
                                for(std::list<struct data_properties>::iterator it2 = data_structures.begin(); it2 != data_structures.end(); it2++) {
                                        if((*it1)->name.compare((*it2).name) == 0){
std::cout<<"Assign3: "<<(*it1)->name<<"\n";
std::cout<<(*it2).name<<"\n";
std::fflush(NULL);
                                                (*it).acc_pat[cur_func.top()][f_inc] += (*it2).acc_pat[cur_func.top()][f_inc];
						(*it).access_pattern[cur_func.top()][f_inc] = assign_access_pattern((*it).acc_pat[cur_func.top()][f_inc]);
                                        }
                                }
                        }
                }
	}
}

void assign_index_prox_while(std::string w_cond, std::string w_body){
std::cout<<"AIP\n";
std::fflush(NULL);
        for(std::list<struct data_properties>::iterator it = data_structures.begin(); it != data_structures.end(); it++) {
                if(!((*it).index_var[cur_func.top()][w_cond].empty())){
                        for(std::list<struct data_properties *>::iterator it1 = (*it).index_var[cur_func.top()][w_cond].begin(); it1 != (*it).index_var[cur_func.top()][w_cond].end(); it1++) {
                                for(std::list<struct data_properties>::iterator it2 = data_structures.begin(); it2 != data_structures.end(); it2++) {
                                        if((*it1)->name.compare((*it2).name) == 0){
std::cout<<"Assign: "<<(*it1)->name<<" "<<(*it2).name<<"\n";
std::fflush(NULL);
                                                (*it).acc_pat[cur_func.top()][w_cond] += (*it2).acc_pat[cur_func.top()][w_cond];
						(*it).access_pattern[cur_func.top()][w_cond] = assign_access_pattern((*it).acc_pat[cur_func.top()][w_cond]);
                                        }
                                }
                        }
                }
		if(!((*it).index_var[cur_func.top()][w_body].empty())){
                        for(std::list<struct data_properties *>::iterator it1 = (*it).index_var[cur_func.top()][w_body].begin(); it1 != (*it).index_var[cur_func.top()][w_body].end(); it1++) {
                                for(std::list<struct data_properties>::iterator it2 = data_structures.begin(); it2 != data_structures.end(); it2++) {
                                        if((*it1)->name.compare((*it2).name) == 0){
std::cout<<"Assign: "<<(*it1)->name<<" "<<(*it2).name<<"\n";
std::fflush(NULL);
                                                (*it).acc_pat[cur_func.top()][w_body] += (*it2).acc_pat[cur_func.top()][w_body];
						(*it).access_pattern[cur_func.top()][w_body] = assign_access_pattern((*it).acc_pat[cur_func.top()][w_body]);
                                        }
                                }
                        }
                }
        }
}


struct data_properties * find_data(std::string name){       /*Find the entry of the variable*/
        for(std::list<struct data_properties>::iterator it = data_structures.begin(); it != data_structures.end(); it++) {
                struct data_properties * dp = &(*it);
                if(((it->cur_def[cur_func.top()]).compare(name)==0)){      /*Current def in current function*/
                        return dp;
                }
                else if((it->name.compare(name)==0)&&(it->scope.compare(cur_scope.top())==0)){        /*Name and current scope*/
                        return dp;
                }
		else if((it->name.compare(name)==0)&&(it->first_def.compare(cur_func.top())==0)){        /*Name and current funtion*/
                        return dp;
                }
                else if(((it->alias[cur_func.top()]).compare(name)==0)){        /*Alias and current function*/
                        return dp;
                }
                else if((it->name.compare(name)==0)&&(it->scope.compare("global")==0)){        /*Name and global*/
                        return dp;
                }
		else if(cflag&&((it->name.compare(name)==0)&&(it->first_def.compare(prev_func)==0))){
			return dp;
		}
		else if(cflag&&((it->alias[prev_func]).compare(name)==0)){        /*Alias and current function*/
                        return dp;
                }
		else{
			for (std::list<std::string>::iterator i = it->prev_def[cur_func.top()].begin(); i != it->prev_def[cur_func.top()].end(); i++){
				if((*i).compare(name)==0){
					return dp;
				}
			}
		}
        }
        return NULL;
    }


    void next_line(std::string fname){	/*Read the net line from the file*/
std::cout<<"nline\n";
std::fflush(NULL);
	std::getline(*(func_map[fname]), cur_line);
	while(cur_line.empty()&&(!(func_map[fname])->eof())){
		std::getline(*(func_map[fname]), cur_line);
	}
std::cout<<cur_line<<"\n";
std::fflush(NULL);
	char * cstr = new char [cur_line.length()+1];
	std::strcpy (cstr, cur_line.c_str());
	cur_word = std::strtok (cstr ," ");
    }

    void cpy_cur_line(){
	cpy_line = std::string(cur_line);
    }

    void next_word(){
	cur_word = std::strtok (NULL, " ");
    }

    void func_call(){	/*Mapping of current defs to call parameters*/
	std::string s(cur_word);
	next_word();
	std::string t_str(cur_word);
	while(t_str.compare(0,1,"@")!=0){	/*Find the function name*/
		next_word();
		t_str = std::string(cur_word);
	}
std::cout<<"call\n";
std::fflush(NULL);
	size_t n = t_str.find("(");
	std::string var_n = t_str.substr(1,n-1);
/*Demangling code goes here*/
	int status = 0;
	//const char *dem_var_n = abi::__cxa_demangle(var_n.c_str(), 0, 0, &status);
	//if(status == 0){
	//	var_n = std::string(dem_var_n);
	//}
	while((t_str.find(")")!=t_str.length()-1)){
        	size_t l = t_str.find("*");     /*Find the pass by reference variables*/
                next_word();
                t_str=std::string(cur_word);
std::cout<<"Par "<<t_str<<"\n";
std::fflush(NULL);
                if(l != std::string::npos){
                	std::string var= t_str.substr(1,t_str.length()-2);
                        struct data_properties * dp = NULL;
			dp = find_data(var);
			if(dp != NULL){
                        	call_parameters.push_back(dp->name);    /*Push the references in a buffer list*/
			}
                }
        }
	
	if((var_n.compare(0,5,"llvm.")!=0)&&(check_func(var_n))){       /*Ignore llvm intrinsic functions*/
		prev_func = cur_func.top();
                cflag = true;
                cur_func.push(var_n);		
	}
	else if(s.find("%") != std::string::npos){
		s = s.substr(1, s.length() - 1);
		struct interim int1;
		int1.var = s;
std::cout<<s<<" func par\n";
std::fflush(NULL);
		while(!call_parameters.empty()){
			int1.dplist.push_front(find_data(call_parameters.front()));
			call_parameters.pop_front();
		}
		temp_vars.push_front(int1);	
	}
	else if((var_n.compare(0,5,"llvm.")==0)||(!check_func(var_n))){
		while(!call_parameters.empty()){
                        call_parameters.pop_front();
                }
	}
		
	next_line(cur_func.top());
    }
 
    void memoryDef(){	/*Perform analysis for store operations*/
std::cout<<"mDef\n";
std::fflush(NULL);
	next_line(cur_func.top());
	std::string t_str(cur_word);
	if(t_str.compare("store")==0){
		std::string first_var;
		std::string fv_name;
		std::string first_type;
		std::string second_type;
		std::string prev_word;
		int i=0;
		while(!t_str.empty()){
			prev_word = t_str;
			next_word();
			t_str=std::string(cur_word);
			if((t_str.find(",")!=std::string::npos)){	/*Find the variable*/
				first_var=t_str.substr(1,t_str.length()-2);
				first_type = prev_word;
				i++;
			}
			if(i==1){
				fv_name = first_var;
std::cout<<"FV: "<<fv_name<<": "<<first_var<<": "<<t_str<<"\n";
std::fflush(NULL);
			}
			if(i==2)
				break;
		}
		std::string var;
		var = t_str.substr(1,t_str.length()-2);
		second_type = prev_word;
		struct data_properties *dp= find_data(var);
		struct data_properties *firstdp = NULL;
		struct interim *tmpv = NULL;
		if(!fv_name.empty()){
			firstdp = find_data(fv_name);
		}
		if(firstdp == NULL){	//Temp Variable removal here
			tmpv = find_temp_var(fv_name);
		}
std::cout<<dp->name<<"\n";
std::fflush(NULL);
		if(mflag&&(dp->size.find("*") != std::string::npos)&&((dp->alloc_type == 's')||(var.find("array")!=std::string::npos))){
			dp->alloc_type = 'd';
/*The code to get source code location for memory region*/
			if(var.find("array") != std::string::npos)
				get_alias(dp, 0, 1);
			else
				get_alias(dp, 0, 0);
			mflag = false;
		}
		if(dp->first_def.empty()){
			dp->first_def = cur_func.top();
		}
		if(iflag&&dp->scope.empty()){
			dp->scope=cur_scope.top();
		}
		if((dp->usage[cur_func.top()][cur_scope.top()].find("w") == std::string::npos)){
			dp->usage[cur_func.top()][cur_scope.top()].append("w");
			if(!dp->pointer_alias.empty()){
				/*updating all pointer aliases*/
				for(std::list<struct data_properties>::iterator it = data_structures.begin(); it != data_structures.end(); it++){
					if((*it).cur_pointer.compare(dp->name) == 0){
						if((dp->usage[cur_func.top()][cur_scope.top()].find("w") == std::string::npos))
							(*it).usage[cur_func.top()][cur_scope.top()].append("w");
					}
				}
			}
		}
		if((dp->size.find("*")!=std::string::npos)&&(firstdp != NULL)&&(firstdp->size.find("*")!=std::string::npos)&&(first_type.find("*")!=std::string::npos)&&(second_type.find("*")!=std::string::npos)){
std::cout<<"PA\n";
std::fflush(NULL);
			firstdp->cur_pointer = std::string(dp->name);
			dp->pointer_alias[cur_func.top()][cur_scope.top()].push_front(firstdp->name);
			firstdp->pointer_alias[cur_func.top()][cur_scope.top()].push_front(dp->name);
			
		}
		if((firstdp != NULL)&&(first_type.find("*")!=std::string::npos)){
			std::list<struct data_properties *>::iterator it1;
                        it1 = std::find(dp->expr_prox[cur_func.top()][cur_scope.top()].begin(), dp->expr_prox[cur_func.top()][cur_scope.top()].end(), firstdp);
                        if((it1 == dp->expr_prox[cur_func.top()][cur_scope.top()].end())&&((firstdp) != (dp))){
				dp->expr_prox[cur_func.top()][cur_scope.top()].push_front(firstdp);	
std::cout<<"FDP: "<<firstdp->name<<"\n";
std::fflush(NULL);
			}
			//dp->expr_prox[cur_func.top()][cur_scope.top()].unique();
		}
		if((tmpv != NULL)&&(first_type.find("*")!=std::string::npos)){
			for(std::list<struct data_properties *>::iterator it = tmpv->dplist.begin(); it != tmpv->dplist.end(); it++){
				std::list<struct data_properties *>::iterator it1;
                                it1 = std::find(dp->expr_prox[cur_func.top()][cur_scope.top()].begin(), dp->expr_prox[cur_func.top()][cur_scope.top()].end(), (*it));
                                if((it1 == dp->expr_prox[cur_func.top()][cur_scope.top()].end())&&((*it) != (dp))){
                        		dp->expr_prox[cur_func.top()][cur_scope.top()].push_front(*it);
std::cout<<"TMPV: "<<(*it)->name<<"\n";
std::fflush(NULL);
				}
                        }
			//dp->expr_prox[cur_func.top()][cur_scope.top()].unique();
			dp->acc_pat[cur_func.top()][cur_scope.top()] += tmpv->acc_pat;
			remove_temp_var(tmpv->var);
                }
	}
	else if((t_str.find("call") != std::string::npos)||(t_str.find("invoke") != std::string::npos)){      /*procedure call*/
		cpy_cur_line();
		if(cpy_line.find("@free(") != std::string::npos){
                        while(t_str.find("free") == std::string::npos){
                                next_word();
                                t_str = std::string(cur_word);
                        }
std::cout<<t_str<<"\n";
std::fflush(NULL);
                        if(t_str.find("free") != std::string::npos){
                                next_word();
                                t_str = std::string(cur_word);
                                std::string vars;
                                vars = t_str.substr(1, t_str.length()-2);
                                struct data_properties *dp= find_data(vars);
                                if(ar_flag){
                                        get_alias(dp, 1, 1);
                                        ar_flag = false;
                                }
                                else{
                                        get_alias(dp, 1, 0);
                                }
                        }
                        next_line(cur_func.top());
                }
		else if(cpy_line.find("malloc") != std::string::npos){
                        mflag = true;
                        malloc_mem_name = t_str.substr(1, t_str.length()-1);
                        next_line(cur_func.top());
                        cpy_cur_line();
                        t_str = std::string(cur_word);
                        if(cpy_line.find("bitcast") != std::string::npos){
                                malloc_mem_name = t_str.substr(1, t_str.length()-1);
                        }
std::cout<<"MMN: "<<malloc_mem_name<<"\n";
std::fflush(NULL);
                }
		else{
                        func_call();
                }
                return;
	}
	next_line(cur_func.top());
    }

    void memoryUse(){	/*Perform analysis for load operations*/
std::cout<<"mUse\n";
std::fflush(NULL);
        next_line(cur_func.top());
	bool bitflag = false;
	bool ar_check = false;
bitcast:
        std::string t_str(cur_word);
	std::string s;
	std::string pt;
	int i = 0;
	while(!t_str.empty()){
		if((t_str.find("%")!=std::string::npos)||(t_str.find("@")!=std::string::npos)){	/*Find the name of the variable*/
			i++;
			if(i == 1){
				s = t_str.substr(1,t_str.length()-1);
			}
			else if(i == 2){
				std::string var;
				if(bitflag){
					var = t_str.substr(1,t_str.length()-1);
				}else{
					var = t_str.substr(1,t_str.length()-2);
				}
				struct data_properties *dp= find_data(var);
				if(bitflag&&(!pt.empty())){
					if(pt.compare(dp->size) != 0){
std::cout<<"ar_flag"<<dp->size<<"\n";
std::fflush(NULL);
						ar_flag = true;
					}
				}
				if((dp->scope.empty()))
                        		dp->scope=cur_scope.top();
				if((dp->first_def.empty())){
					dp->first_def = cur_func.top();
				}
				if((dp->usage[cur_func.top()][cur_scope.top()].find("r") == std::string::npos)){
					dp->usage[cur_func.top()][cur_scope.top()].append("r");
					if(!(dp->pointer_alias.empty())){
                                	/*updating all pointer aliases*/
						for(std::list<struct data_properties>::iterator it = data_structures.begin(); it != data_structures.end(); it++){
                                        		if((*it).cur_pointer.compare(dp->name) == 0){
								if((dp->usage[cur_func.top()][cur_scope.top()].find("r") == std::string::npos))
                                                			(*it).usage[cur_func.top()][cur_scope.top()].append("r");
                                        		}
                                		}
                        		}
				}
				if(!dp->cur_def[cur_func.top()].empty()){
					dp->prev_def[cur_func.top()].push_front(dp->cur_def[cur_func.top()]); 
				}
				dp->cur_def[cur_func.top()] = s;	/*Add the current definition*/
				break;
                	}
		}
		if(t_str.compare("bitcast") == 0){
std::cout<<"bituse\n";
std::fflush(NULL);
			ar_check = true;
		}
		next_word();
		t_str = std::string(cur_word);
		if(ar_check){
std::cout<<"bitp\n";
std::fflush(NULL);
			pt = std::string(cur_word);
			ar_check = false;
		}
	}
	next_line(cur_func.top());
	cpy_cur_line();
	if(cpy_line.find("bitcast") != std::string::npos){
		bitflag = true;
		goto bitcast;
	}
    }

    void arrays(){	/*Perform array def analysis*/
std::cout<<"arrays\n";
std::fflush(NULL);
	struct data_properties *dp;
	int i = 0;
	std::string t_str(cur_word);
	next_word();
	std::string s(cur_word);
	while(!s.empty()){
		if((s.find("%")!=std::string::npos)||(s.find("@")!=std::string::npos)){	/*Find the array name*/
			if(i == 0){
				dp = find_data(s.substr(1, s.length()-2));	/*Fetch the correct entry*/
				if(!dp->cur_def[cur_func.top()].empty()){
                                        dp->prev_def[cur_func.top()].push_front(dp->cur_def[cur_func.top()]);
                                }
				dp->cur_def[cur_func.top()] = t_str.substr(1,t_str.length()-1);
std::cout<<dp->name<<" "<<t_str<<"\n";
std::fflush(NULL);
				if(t_str.find("decay")!=std::string::npos){
					break;
				}
				i++;
			}else if(i == 1){
				struct interim * interim1 = find_temp_var(s.substr(1, s.length()-1));
				for(std::list<struct data_properties *>::iterator it = interim1->dplist.begin(); it != interim1->dplist.end(); it++){
					std::list<struct data_properties *>::iterator it1;
					it1 = std::find(dp->index_var[cur_func.top()][cur_scope.top()].begin(), dp->index_var[cur_func.top()][cur_scope.top()].end(), (*it));
					if(it1 == dp->index_var[cur_func.top()][cur_scope.top()].end()){
						dp->index_var[cur_func.top()][cur_scope.top()].push_front(*it);
					}
					it1 = std::find(dp->expr_prox[cur_func.top()][cur_scope.top()].begin(), dp->expr_prox[cur_func.top()][cur_scope.top()].end(), (*it));
                                        if((it1 == dp->expr_prox[cur_func.top()][cur_scope.top()].end())&&((dp) != (*it))){
						dp->expr_prox[cur_func.top()][cur_scope.top()].push_front(*it);
std::cout<<"TVA: "<<(*it)->name<<"\n";
std::fflush(NULL);
					}
				}
				//dp->expr_prox[cur_func.top()][cur_scope.top()].unique();
				dp->acc_pat[cur_func.top()][cur_scope.top()] += interim1->acc_pat;
				remove_temp_var(interim1->var);
				break;	
			}
		}
		next_word();
		s = std::string(cur_word);
	}
	next_line(cur_func.top());
    }

    void mathop(){
	std::string t_str(cur_word);
	std::string s(t_str.substr(1,t_str.length()-1));
std::cout<<"MathOp: "<<s<<"\n";
std::fflush(NULL);
	next_word();
	std::string op1(cur_word);
	while(op1.find("%")==std::string::npos){
		next_word();
		op1 = std::string(cur_word);
	}
	if((t_str.compare(1,3,"idx")==0)||(t_str.compare(1,4,"conv")==0)){
		op1 = std::string(op1.substr(1,op1.length()-1));
	}
	else{
		op1 = std::string(op1.substr(1,op1.length()-2));
	}
std::cout<<"OP 1: "<<op1<<"\n";
std::fflush(NULL);
	struct data_properties *dp1 = find_data(op1);
	struct data_properties *dp2 = NULL;
	struct interim * interim1 = NULL;
	struct interim * interim2 = NULL;
	if(dp1 == NULL){
		interim1 = find_temp_var(op1);
std::cout<<"Int 1: "<<interim1->var<<"\n";
std::fflush(NULL);
	}
	if((t_str.compare(1,3,"idx")==0)||(t_str.compare(1,4,"conv")==0)){
		if(interim1 == NULL){
			struct interim int_var;
			int_var.var = std::string(s);
			int_var.dplist.push_front(dp1);
			temp_vars.push_front(int_var);
		}
		if(dp1 == NULL){
			interim1->var = std::string(s);
		}
		next_line(cur_func.top());
		return;
	}
	std::string op2;
	std::string::size_type sz;
	int op2_num;
	if((t_str.compare(1,3,"add")==0)||(t_str.compare(1,3,"sub")==0)||(t_str.compare(1,3,"mul")==0)||(t_str.compare(1,3,"div")==0)||(t_str.compare(1,3,"rem")==0)||(t_str.compare(1,3,"inc")==0)||(t_str.compare(1,3,"dec")==0)||(t_str.compare(1,3,"cmp")==0)){
		next_word();
                op2 = std::string(cur_word);
                if(std::isdigit(cur_word[0]) == 0){
std::cout<<"OP2 "<<op2<<"\n";
std::fflush(NULL);
                        op2 = op2.substr(1,op2.length()-1);
                        dp2 = find_data(op2);
                        if(dp2 == NULL){
                               	interim2 = find_temp_var(op2);
                        }
                }
                else{
                        op2_num = std::stoi(op2);
                }
		struct interim int_var;
                int_var.var = s;
		int_var.acc_pat += op_val[t_str.substr(1,3)];
		if(dp1 != NULL){
                        int_var.dplist.push_front(dp1);
		}
		if((dp1 == NULL)&&(interim1 != NULL)){
std::cout<<"Empty dp1\n";
std::fflush(NULL);
			if(!interim1->dplist.empty()){
				for(std::list<struct data_properties *>::iterator it = interim1->dplist.begin(); it != interim1->dplist.end(); it++){
                        		int_var.dplist.push_front(*it);
                        	}
			}
			int_var.acc_pat += interim1->acc_pat;
			remove_temp_var(interim1->var);
		}
		if(dp2 != NULL){
std::cout<<"DP2 "<<dp2->name<<"\n";
std::fflush(NULL);
			int_var.dplist.push_front(dp2);
		}
		if((dp2 == NULL)&&(interim2 != NULL)){
std::cout<<"Empty dp2\n";
std::fflush(NULL);
			if(!interim2->dplist.empty()){
				for(std::list<struct data_properties *>::iterator it = interim2->dplist.begin(); it != interim2->dplist.end(); it++){
                        	        int_var.dplist.push_front(*it);
                        	}
			}
			int_var.acc_pat += interim2->acc_pat;
			remove_temp_var(interim2->var);
		}
		if((dp2 == NULL)&&(interim2 == NULL)&&(op2_num != 1)){
std::cout<<"OP2 NUM\n";
std::fflush(NULL);
			int_var.acc_pat += 5;
		}
		temp_vars.push_front(int_var);
	}
	next_line(cur_func.top());
    }

    void parameter_map(){	/*Parameter mapping after every function call*/
std::cout<<"pmap\n";
std::fflush(NULL);
	next_line(cur_func.top());
	next_word();
	std::string t_str=std::string(cur_word);
	while(t_str.compare(0,1,"@")!=0){	/*Iterate till the function name*/
                next_word();
                t_str = std::string(cur_word);
        }
	bool pflag;
	while((t_str.find(")")!=t_str.length()-1)){
		pflag=false;
                size_t l = t_str.find("*");	/*Look for pass by reference*/
                next_word();
               	t_str=std::string(cur_word);
		std::string par = t_str.substr(1,t_str.length()-2);
		struct data_properties * dp = NULL;
		if(((cur_func.top()).compare("main")!=0)&&(l != std::string::npos)){
                       	std::string var = call_parameters.front();	/*Parameter name in previous function*/
			call_parameters.pop_front();
			dp = find_data(var);
		}
		if(dp == NULL){		/*Create new record if it doesn't exists*/
			struct data_properties data_t;
			data_t.name = par;
			data_t.first_def = cur_func.top();
                       	data_t.scope = cur_func.top();
			dp = &data_t;
			pflag=true;
		}
                if((cur_func.top()).compare("main")==0){
                        dp->usage[cur_func.top()]["commandline"] = "";
		}else{
std::cout<<"Here "<<dp->name<<"\n";
std::fflush(NULL);
                        dp->usage[cur_func.top()][cur_func.top()] = "";
		}

		dp->alias[cur_func.top()] = par;
		if(pflag){
		std::cout<<"Per "<<dp->d_scope<<"\n";
		std::fflush(NULL);
			data_structures.push_front(*dp);
		}
		if(t_str.find(")")==(t_str.length()-1)){
			break;
		}
                next_word();
               	t_str=std::string(cur_word);
        }
	next_line(cur_func.top());
	cflag = false;
    }

    void forEnd(){	/*End the for scope*/
std::cout<<"forend\n";
std::fflush(NULL);
	std::string t_str(cur_word);
	while(t_str.compare(1,8,"for.cond")!=0){	/*Find the beginning of this for scope*/
		next_word();
		t_str=std::string(cur_word);
	}	
	std::string lab_n = t_str.substr(1,t_str.length());
	std::string f_body;
	std::string f_inc;
	std::string f_cond;
	while(lab_n.compare(cur_scope.top())!=0){
std::cout<<t_str<<" "<<lab_n<<" "<<cur_scope.top()<<"\n";
std::fflush(NULL);
		if((cur_scope.top()).find("for.body") != std::string::npos){
			f_body = cur_scope.top();
		}
		if((cur_scope.top()).find("for.inc") != std::string::npos){
			f_inc = cur_scope.top();
                }
		cur_scope.pop();
	}
	if((cur_scope.top()).find("for.cond") != std::string::npos){
        	f_cond = cur_scope.top();
        }
	cur_scope.pop();	/*Pop the for scope*/
	assign_index_prox_for(f_cond, f_body, f_inc);
	while(lab_n.compare(nest_scope.top())!=0){
std::cout<<t_str<<" "<<lab_n<<" "<<cur_scope.top()<<"\n";
std::fflush(NULL);
		nest_scope.pop();
	}
	nest_scope.pop();	/*Pop the for scope*/
	for_scope_no--;
	next_line(cur_func.top());
    }

    void whileEnd(){	/*End the  while scope*/
std::cout<<"whileend\n";
std::fflush(NULL);
        std::string t_str(cur_word);
        while(t_str.compare(1,10,"while.cond")!=0){	/*Find the beginning of this while scope*/
                next_word();
                t_str=std::string(cur_word);
        }
        std::string lab_n = t_str.substr(1,t_str.length()-1);
	std::string w_body;
	std::string w_cond;
        while(lab_n.compare(cur_scope.top())!=0){
		if((cur_scope.top()).find("while.body") != std::string::npos){
                        w_body = cur_scope.top();
                }
		cur_scope.pop();
        }
	if((cur_scope.top()).find("while.cond") != std::string::npos){
        	w_cond = cur_scope.top();
        }
	cur_scope.pop();	/*Pop the while scope*/
	assign_index_prox_while(w_cond, w_body);
        while(lab_n.compare(nest_scope.top())!=0){
		nest_scope.pop();
        }
	nest_scope.pop();	/*Pop the while scope*/
	while_scope_no--;
        next_line(cur_func.top());
    }

    void ifEnd(){	/*End the if scope*/
std::cout<<"ifend\n";
std::fflush(NULL);
	bool if_stack_flag = false;
        std::string t_str(cur_word);
	while(t_str.compare(1,7,"if.then")!=0){	/*Find the beginning of this if scope*/
                next_word();
                t_str=std::string(cur_word);
        }
        std::string lab_n = t_str.substr(1,t_str.length()-1);
        while(lab_n.compare(cur_scope.top())!=0){
		cur_scope.pop();
        }
	cur_scope.pop();		/*Pop the scope*/
	if(nest_scope.top().find("if.else") != std::string::npos){
		if_stack_flag = true;
	}
        while(lab_n.compare(nest_scope.top())!=0){
		if(if_stack_flag){
			if_stack_bal--;
		}
		nest_scope.pop();
        }
	if(if_stack_flag){
                if_stack_bal--;
        }
	nest_scope.pop();		/*Pop the scope*/
	if_scope_no--;
        next_line(cur_func.top());
    }



    void entryVars(){		/*Map the variables declared at the entry of a procedure*/
std::cout<<"eV\n";
std::fflush(NULL);
	next_line(cur_func.top());
	std::string t_str(cur_word);
	while(t_str.compare(0,1,"%")==0){	/*Loop over all variables*/
		std::string var_n = t_str.substr(1,t_str.length()-1);
		if(var_n.find(".addr")!=std::string::npos){	/*Pass by reference variables*/
			struct data_properties * dt = find_data(var_n.substr(0,var_n.length()-5));
std::cout<<dt->name<<"\n";
std::fflush(NULL);
			dt->alias[cur_func.top()] = var_n;
		}
		else{				/*Other variables in the procedure scope*/
			struct data_properties data_t;	
                	data_t.name = var_n;
			data_t.first_def = cur_func.top();
			data_t.alias[cur_func.top()] = var_n;
                	data_t.scope = cur_func.top();
			data_t.usage[cur_func.top()][cur_scope.top()] = "";
			while(t_str.compare("alloca") != 0){
std::cout<<"IN"<<"\n";
std::fflush(NULL);
				next_word();
				t_str = std::string(cur_word);
			}
			next_word();
                        t_str = std::string(cur_word);
			data_t.size = t_str.substr(0, (t_str.length() - 1));
                	data_structures.push_front(data_t);
		}
		next_line(cur_func.top());
		t_str = std::string(cur_word);
	}
    }

    void analyze_line(){	/*Loop over every line of the Memory SSA and take the required action*/

std::cout<<"al"<<cur_word<<"\n";
std::fflush(NULL);
        while (cur_word != NULL)
        {
    		std::string t_str(cur_word);
    		if((t_str.compare(";"))==0){
			while(cur_word != NULL){
				std::string it_str(cur_word);
				if(it_str.compare(0,9,"MemoryDef")==0){	/*Store operations*/
					if(it_str.compare(10,11,"liveOnEntry")==0)
						iflag = true;
					memoryDef();
					return;
				}
				else if(it_str.compare(0,9,"MemoryPhi")==0){
					iflag = false;
                                        next_line(cur_func.top());
                                        return;
                                }
				else if(it_str.compare(0,9,"MemoryUse")==0){	/*Load operations*/
                                        memoryUse();
                                        return;
                                }
				else if(it_str.compare(0,8,"Function")==0){
					parameter_map();	/*Map the pass by reference parameters of the function definition to the calling function variables*/
					return;
				}
				next_word();
			}
		}
		else if(t_str.compare(0,6,"%array")==0){	/*Analyze the array operations */
			arrays();
			return;
		}
		else if((t_str.compare(0,8,"for.cond")==0)||(t_str.compare(0,8,"for.body")==0)||(t_str.compare(0,7,"for.inc")==0)||(t_str.compare(0,7,"if.then")==0)||(t_str.compare(0,7,"if.else")==0)||(t_str.compare(0,10,"while.cond")==0)||(t_str.compare(0,10,"while.body")==0)){	/* Push the new scope*/
			cur_scope.push(t_str.substr(0,t_str.length()-1));
			if((t_str.find("cond")!=std::string::npos)||(t_str.compare(0,7,"if.then")==0)||(t_str.compare(0,7,"if.else")==0)){
				if(((t_str.compare(0,7,"if.else")==0)&&((nest_scope.top()).find("if.then") != std::string::npos))||((t_str.compare(0,7,"if.then")==0)&&((nest_scope.top()).find("if.else") != std::string::npos)))
					if_stack_bal++;
				nest_scope.push(t_str.substr(0,t_str.length()-1));
				get_nesting();
//Need to fix the IF scope evaluation.
//if.then with no if.else to follow if()
//if.then with if.else to follow else if()
//if.else with no if.then to follow then else
			}
			next_line(cur_func.top());
			return;
		}
		else if(t_str.compare(0,7,"for.end")==0){	/*Close the for scope*/
			forEnd();
			return;
		}
		else if(t_str.compare(0,9,"while.end")==0){	/*Close the while scope*/
                        whileEnd();
                        return;
                }
		else if(t_str.compare(0,6,"if.end")==0){ /*Close the if scope*/
                        ifEnd();
                        return;
                }
		else if((t_str.compare(1,3,"add")==0)||(t_str.compare(1,3,"sub")==0)||(t_str.compare(1,3,"mul")==0)||(t_str.compare(1,3,"div")==0)||(t_str.compare(1,3,"rem")==0)||(t_str.compare(1,3,"idx")==0)||(t_str.compare(1,3,"inc")==0)||(t_str.compare(1,3,"cmp")==0)||(t_str.compare(1,4,"conv")==0)){
			mathop();
			return;
		}
		else if(t_str.compare(0,4,"call")==0){	/*Analyze the call to a function and map the passed by reference variables*/
			func_call();
			return;
		}
		else if(t_str.compare("entry:")==0){	/*Map the variables in the current scope at its entry*/
			if(scope_base != -1){
				prev_scope_base.push(scope_base);
			}
			scope_base = nest_scope.size();
std::cout<<"Scope base: "<<scope_base<<"\n";
std::fflush(NULL);
			cur_scope.push(t_str);
			nest_scope.push(t_str);
			get_nesting();
			entryVars();
			return;
		}
		else if(t_str.compare("ret")==0){	/*End of current scope*/
            		cur_scope.pop();
            		nest_scope.pop();
			if(scope_base != 0){
				scope_base = prev_scope_base.top();
			}
			prev_scope_base.pop();
			next_line(cur_func.top());
                        return;
                }
		else{
			next_line(cur_func.top());
			return;
		}
  	}
    }

    void control_flow_traversal(){	/*Traverse the control flow of the program starting with main*/
std::cout<<"cft\n";
std::fflush(NULL);
	cur_func.push("main");
	while(!cur_func.empty()){
	next_line(cur_func.top());
		while(!func_map[cur_func.top()]->eof()){
			analyze_line();
		}
std::cout<<"pop\n";
std::fflush(NULL);
		cur_func.pop();
		while_scope_no = type_scores.top();
		type_scores.pop();
		if_scope_no = type_scores.top();
		type_scores.pop();
		for_scope_no = type_scores.top();
		type_scores.pop();
		type_scores.pop();
	}
    }

    void make_decision(){	/*Need to fix this*/
	for(std::list<struct data_properties>::iterator it = data_structures.begin(); it != data_structures.end(); it++) {
		struct data_properties *dp = &(*it);
		get_scope(dp);
		std::cout<<"Data structure: "<<(*it).name<<" Scope: "<<(*it).scope<<" First Def: "<<(*it).first_def<<"\n";
		std::cout<<"Definitions:\n";
		for(std::map<std::string, std::string>::iterator it1 = ((*it).cur_def).begin(); it1 != ((*it).cur_def).end(); it1++){
			std::cout<<"\tCur Func: "<<(*it1).first<<" Cur Def: "<<(*it1).second<<"\n";
		}
		std::cout<<"Aliasing:\n";
		for(std::map<std::string, std::string>::iterator it2 = ((*it).alias).begin(); it2 != ((*it).alias).end(); it2++){
			std::cout<<"\tCur Func: "<<(*it2).first<<" Alias: "<<(*it2).second<<"\n";
        	}
		std::cout<<"Pointer Aliasing:\n";
                for(std::list<struct memory_region>::iterator it2 = ((*it).alloc_info).begin(); it2 != ((*it).alloc_info).end(); it2++){
                        std::cout<<"\tAlloc Func: "<<(*it2).alloc_func<<" Alloc Scope: "<<(*it2).alloc_scope<<" Alloc File: "<<(*it2).alloc_file<<"\n"<<"\tFree Func: "<<(*it2).free_func<<" Free Scope: "<<(*it2).free_scope<<" Free File: "<<(*it2).free_file<<"\n"<<"\tArray Ref: "<<(*it2).array_ref<<" Alloc Line: "<<(*it2).alloc_line<<" Alloc Column: "<<(*it2).alloc_col<<" Free Line: "<<(*it2).free_line<<" Free Column: "<<(*it2).free_col<<"\n";
                }
	std::cout<<"Usage:\n";
                for(std::map<std::string, std::map<std::string, std::string>>::iterator it3 = ((*it).usage).begin(); it3 != ((*it).usage).end(); it3++){
                	std::cout<<"\tFunc: "<<(*it3).first<<"\n";
			for(std::map<std::string, std::string>::iterator it4 = ((*it3).second).begin(); it4 != ((*it3).second).end(); it4++){
				std::cout<<"\t\tScope: "<<(*it4).first<<" Usage: "<<(*it4).second<<"\n";
				std::string u = (*it4).second;
				if(!u.compare("r")){
					
				}
			}
			
        	}
	std::cout<<"Prev Def:\n";
                for(std::map<std::string, std::list<std::string>>::iterator it5 = ((*it).prev_def).begin(); it5 != ((*it).prev_def).end(); it5++){
                	std::cout<<"\tFunc: "<<(*it5).first<<"\n";
                	for(std::list<std::string>::iterator it6 = ((*it5).second).begin(); it6 != ((*it5).second).end(); it6++){
                        	std::cout<<"\t\tDef: "<<(*it6)<<"\n";
                	}
		}
	std::cout<<"Access Pattern:\n";
                for(std::map<std::string, std::map<std::string, std::string>>::iterator it3 = ((*it).access_pattern).begin(); it3 != ((*it).access_pattern).end(); it3++){
                        std::cout<<"\tFunc: "<<(*it3).first<<"\n";
                        for(std::map<std::string, std::string>::iterator it4 = ((*it3).second).begin(); it4 != ((*it3).second).end(); it4++){
                                std::cout<<"\t\tScope: "<<(*it4).first<<" Pattern: "<<(*it4).second<<"\n";
                        }

                }
        std::cout<<"Expr Proximity:\n";
                for(std::map<std::string, std::map<std::string, std::list<struct data_properties *>>>::iterator it3 = ((*it).expr_prox).begin(); it3 != ((*it).expr_prox).end(); it3++){
                        std::cout<<"\tFunc: "<<(*it3).first<<"\n";
                        for(std::map<std::string, std::list<struct data_properties *>>::iterator it4 = ((*it3).second).begin(); it4 != ((*it3).second).end(); it4++){
                                std::cout<<"\t\tScope: "<<(*it4).first<<"\n";
                                for(std::list<struct data_properties *>::iterator it5 = ((*it4).second).begin(); it5 != ((*it4).second).end(); it5++){
                                        std::cout<<"\t\t\tProx var: "<<(*it5)->name<<"\n";
                                }
                        }

                }

        std::cout<<"Index Var:\n";
                for(std::map<std::string, std::map<std::string, std::list<struct data_properties *>>>::iterator it3 = ((*it).index_var).begin(); it3 != ((*it).index_var).end(); it3++){
                        std::cout<<"\tFunc: "<<(*it3).first<<"\n";
                        for(std::map<std::string, std::list<struct data_properties *>>::iterator it4 = ((*it3).second).begin(); it4 != ((*it3).second).end(); it4++){
                                std::cout<<"\t\tScope: "<<(*it4).first<<"\n";
                                for(std::list<struct data_properties *>::iterator it5 = ((*it4).second).begin(); it5 != ((*it4).second).end(); it5++){
                                        std::cout<<"\t\t\tIndex var: "<<(*it5)->name<<"\n";
                                }
                        }

                }
	std::cout<<(*it).file<<" "<<(*it).func<<" "<<(*it).d_scope<<" "<<(*it).decl_line<<" "<<(*it).decl_col<<" "<<(*it).start_line<<" "<<(*it).start_column<<" "<<(*it).end_line<<" "<<(*it).end_column<<"\n";	
	}
	for(std::list<struct scope_info>::iterator it = scope_list.begin(); it != scope_list.end(); it++){
		std::cout<<(*it).name<<" "<<(*it).file<<" "<<(*it).func<<" "<<(*it).start_line<<" "<<(*it).start_column<<" "<<(*it).end_line<<" "<<(*it).end_column<<" "<<(*it).abs_nesting_score<<" "<<(*it).type_nesting_score<<"\n";
	}
    }

    void file_cleanup(){	/*Remove the temp files*/
	std::string rm = "rm";
	for (std::map<std::string, std::ifstream *>::iterator it=func_map.begin(); it!=func_map.end(); ++it){
		rm = rm +" "+ it->first;
	}
	std::system(rm.c_str());
	std::system("rm pointeralias");
	std::system("rm vardeclscope");
	std::system("rm nestingscore");
    }

  struct IDP : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    IDP() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
	/*Create entries for global variables*/
	std::error_code ec (0, std::generic_category());
	for(Module::global_iterator gi = M.global_begin(), gi_end = M.global_end(); gi != gi_end; gi++){
		struct data_properties data_t;
		data_t.name = (*gi).getName();
		data_t.scope = "global";
		data_structures.push_front(data_t);
	}
	for(Module::iterator gfl_iterator = M.begin(), gfl_end = M.end(); gfl_iterator != gfl_end; gfl_iterator++){
		if(gfl_iterator->isDeclaration())
			continue;
		auto &MSSA = getAnalysis<MemorySSAWrapperPass>(*gfl_iterator).getMSSA();
		raw_ostream * OS = new raw_fd_ostream(gfl_iterator->getName(), ec, sys::fs::F_Append); 
		MSSA.print(*OS);
		func_map.insert(std::pair<std::string, std::ifstream *>(gfl_iterator->getName(), new std::ifstream(gfl_iterator->getName())));
	}

	
	assign_op_val();
	get_numa_info();
	control_flow_traversal();
	make_decision();
	file_cleanup();

      return false;
    }

    // We don't modify the program, so we preserve all analyses.
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
      AU.addRequired<MemorySSAWrapperPass>();
    }
  };
}

char IDP::ID = 0;
static RegisterPass<IDP>
X("IDP", "Intelligent Data Pass (with getAnalysisUsage implemented)");
