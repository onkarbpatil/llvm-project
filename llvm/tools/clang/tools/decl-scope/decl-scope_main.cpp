#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <list>
#include <string>
#include <sstream>
#include <memory>
#include <iostream>
#include <fstream>

struct decl_scope_list{
	std::string name;
	std::string func;
	std::string class_field;
        std::string scope;
        std::string file;
	int decl_line;
	int decl_col;           
	int start_line;
	int start_column;
	int end_line;
        int end_column;
};

struct scope{
	std::string name;
	std::string file;
	std::string func;
	std::string class_field;
	int start_line;
        int start_column;
        int end_line;
        int end_column;
	int nesting_score = 0;
};

struct malloc_list{
	std::string name;
        std::string alloc_func;
        std::string alloc_scope;
        std::string alloc_file;
        std::string free_func;
        std::string free_scope;
        std::string free_file;
	int array_ref = 0;
        int alloc_line = -1;
        int alloc_column = -1;
        int free_line = -1;
        int free_column = -1;
};

std::list<struct scope> cur_scope;
std::list<struct decl_scope_list> var_list;
std::list<struct malloc_list> alloc_list;
std::string cur_func = std::string("global");
std::string cur_class;
struct scope scope_now;

void eval_nesting(){
	for(std::list<struct scope>::iterator it = cur_scope.begin(); it != cur_scope.end(); it++){
		for(std::list<struct scope>::iterator it1 = cur_scope.begin(); it1 != cur_scope.end(); it1++){
			if((*it).start_line >= (*it1).start_line){
			 if(((*it).start_line == (*it1).start_line)&&((*it).start_column < (*it1).start_column)){
				continue;
			  }
			  if(((*it).end_line <= (*it1).end_line)){
                  	   if(((*it).end_line == (*it1).end_line)&&((*it).end_column >= (*it1).end_column)){
                        	continue;
                  	  }
			  (*it).nesting_score++;
			 }
			}
		}
	}
}

void scope_eval(struct decl_scope_list *s){
//std::cout<<"scope eval\n";
//fflush(NULL);
/*	if((cur_scope.top()).name.compare("global") == 0){
		cur_scope.push(*s);
	}
	else if((cur_scope.top()).end_line > s->end_line){
		cur_scope.push(*s);
	}
	else if(((cur_scope.top()).end_line == s->end_line)&&((cur_scope.top()).end_column >= s->end_column)){
		cur_scope.push(*s);
	}
	else if((cur_scope.top()).end_line < s->end_line){
		while((cur_scope.top()).end_line < s->end_line){
			cur_scope.pop();
		}
		if((cur_scope.top()).end_line == s->end_line){
			while((cur_scope.top()).end_column < s->end_column){
                		cur_scope.pop();
			}
        	}
		//cur_scope.push_front(*s);
	}
*/
	for (std::list<struct scope>::iterator it = cur_scope.begin(); it != cur_scope.end(); it++){
		if((s->decl_line >= (*it).start_line)){
		 if((s->decl_line == (*it).start_line)&&(s->decl_col < (*it).start_column)){
			continue;
		 }
//std::cout<<"1 "<<s->decl_line<<" "<<s->decl_col<<"\n";
		 if((s->decl_line <= (*it).end_line)){
		  if((s->decl_line == (*it).end_line)&&(s->decl_col >= (*it).end_column)){
		   	continue;
		  }
//std::cout<<"2\n";
		  if((s->start_line <= (*it).start_line)){
		   if((s->start_line == (*it).start_line)&&(s->start_column < (*it).start_column)){
		    	continue;
		   }
//std::cout<<"3\n";
		   if((s->end_line >= (*it).end_line)){
		    if((s->end_line == (*it).end_line)&&(s->end_column < (*it).end_column)){
			continue;
		    }
//std::cout<<"4\n";
			s->scope = (*it).name;
			s->start_line = (*it).start_line;
			s->start_column = (*it).start_column;
			s->end_line = (*it).end_line;
                        s->end_column = (*it).end_column; 
		   }
		  }
		 }
		}
	}
}

void parseloc(struct scope * s, std::string loc, int f){
	char * floc = new char[loc.length()+1];
	std::strcpy(floc,loc.c_str());
	s->file = std::string(std::strtok (floc ,":"));
	if(f == 0){
		s->start_line = std::atoi(std::strtok (NULL ,":"));
		s->start_column = std::atoi(std::strtok (NULL ,":"));
//std::cout<<"A\n";
//fflush(NULL);
	}else if(f == 1){
		s->end_line = std::atoi(std::strtok (NULL ,":"));
                s->end_column = std::atoi(std::strtok (NULL ,":"));
	}
}

void parseloc_malloc(struct malloc_list * s, std::string loc, int f){
std::cout<<"parseloc_malloc\n";
std::fflush(NULL);
        char * floc = new char[loc.length()+1];
        std::strcpy(floc,loc.c_str());
        if(f == 0){
        	s->alloc_file = std::string(std::strtok (floc ,":"));
                s->alloc_line = std::atoi(std::strtok (NULL ,":"));
                s->alloc_column = std::atoi(std::strtok (NULL ,":"));
//std::cout<<"A\n";
////fflush(NULL);
        }else if(f == 1){
		for(std::list<struct decl_scope_list>::iterator it = var_list.begin(); it != var_list.end(); it++){
			if((s->name.compare((*it).name) == 0)&&((s->alloc_func.compare((*it).func)==0)||((*it).scope.compare("global")))){
        			s->free_file = std::string(std::strtok (floc ,":"));
				s->free_line = (*it).end_line;
				s->free_column = (*it).end_column;
				break;
			}
		}
        //	s->free_line = std::atoi(std::strtok (NULL ,":"));
         //       s->free_column = std::atoi(std::strtok (NULL ,":"));
        }
	else if(f == 2){
        	s->free_file = std::string(std::strtok (floc ,":"));
		s->free_line = std::atoi(std::strtok (NULL ,":"));
                s->free_column = std::atoi(std::strtok (NULL ,":"));
std::cout<<"pm\n";
std::fflush(NULL);
	}
}

struct malloc_list * search_malloc(std::string s){
	for(std::list<struct malloc_list>::iterator it = alloc_list.begin(); it != alloc_list.end(); it++){
		if((*it).name.compare(s) == 0){
std::cout<<"share_malloc\n";
std::fflush(NULL);
			return (&(*it));
		}
	}
	return NULL;
}

void var_loc(struct decl_scope_list *v, std::string loc){
        char * floc = new char[loc.length()+1];
        std::strcpy(floc,loc.c_str());
	v->file = std::string(std::strtok (floc ,":"));
	v->decl_line = std::atoi(std::strtok (NULL ,":"));
        v->decl_col = std::atoi(std::strtok (NULL ,":"));
}

static llvm::cl::extrahelp
    CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
llvm::cl::OptionCategory FindDeclCategory("decl-scope options");

static char FindDeclUsage[] = "decl-scope <source file>";

class DeclVisitor : public clang::RecursiveASTVisitor<DeclVisitor> {
  clang::SourceManager &SourceManager;

public:
  DeclVisitor(clang::SourceManager &SourceManager)
      : SourceManager(SourceManager) {}

  bool traverseBody(clang::Stmt * stmts){
		for(clang::Stmt::child_iterator i = stmts->child_begin(); i != stmts->child_end(); i++ ){
			struct scope s;
std::cout<<"Sts: "<<std::string((*i)->getStmtClassName())<<"\n";
std::fflush(NULL);
			if(std::string((*i)->getStmtClassName()).find("CallExpr") != std::string::npos){
				struct malloc_list *m = NULL;	
				bool a_flag = false;
                                if(((((((clang::CallExpr *)(*i))->getDirectCallee())->getNameInfo()).getName()).getAsString()).compare("free") == 0){
					if(std::string((((clang::Stmt *)((((clang::CallExpr *)(*i))->getArg(0))->IgnoreCasts()))->getStmtClassName())).find("ArraySubscriptExpr") != std::string::npos){
						//m = search_malloc((((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::CallExpr *)(*i))->getArg(0))->IgnoreCasts()))->getBase()->IgnoreCasts())->getNameInfo().getName().getAsString()));
						if(m == NULL){
							struct malloc_list mt;
							m = &mt;
							a_flag = true;	
						}
						m->name = std::string(((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::CallExpr *)(*i))->getArg(0))->IgnoreCasts()))->getBase()->IgnoreCasts())->getNameInfo().getName().getAsString());
std::cout<<m->name<<"\n";
std::fflush(NULL);
						m->free_func = cur_func;
                                                m->free_scope = scope_now.name;
                                                m->free_file = scope_now.file;
						m->array_ref = 1;
std::cout<<"IN_malloc1\n";
std::fflush(NULL);
						parseloc_malloc(m, getDeclLocation(((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::CallExpr *)(*i))->getArg(0))->IgnoreCasts()))->getBase()->IgnoreCasts())->getBeginLoc()), 2);
                                                //parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts())->getEndLoc()), 1);
						
						//std::cout<<((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::CallExpr *)(*i))->getArg(0))->IgnoreCasts()))->getBase()->IgnoreCasts())->getNameInfo().getName().getAsString()<<"\n";
						
					}
					if(std::string((((clang::Stmt *)((((clang::CallExpr *)(*i))->getArg(0))->IgnoreCasts()))->getStmtClassName())).find("DeclRefExpr") != std::string::npos){
						//struct malloc_list *m = NULL;
                                                //m = search_malloc(((clang::DeclRefExpr *)(((clang::CallExpr *)(*i))->getArg(0)->IgnoreCasts()))->getNameInfo().getName().getAsString());
                                                if(m == NULL){
                                                        struct malloc_list mt;
                                                        m = &mt;
							a_flag = true;	
                                                }
						m->name = std::string(((clang::DeclRefExpr *)(((clang::CallExpr *)(*i))->getArg(0)->IgnoreCasts()))->getNameInfo().getName().getAsString());
std::cout<<m->name<<"\n";
std::fflush(NULL);
                                                m->free_func = cur_func;
                                                m->free_scope = scope_now.name;
                                                m->free_file = scope_now.file;
std::cout<<"IN_malloc2\n";
std::fflush(NULL);
                                                parseloc_malloc(m, getDeclLocation(((clang::DeclRefExpr *)(((clang::CallExpr *)(*i))->getArg(0)->IgnoreCasts()))->getBeginLoc()), 2);
						//std::cout<<((clang::DeclRefExpr *)((((clang::CallExpr *)(*i))->getArg(0))->IgnoreCasts()))->getNameInfo().getName().getAsString()<<"\n";
					}
					if(a_flag){
						m->alloc_func = std::string("nofunc");
						m->alloc_scope = std::string("noscope");
						m->alloc_file = std::string("nofile");
						alloc_list.push_back(*m);
std::cout<<"aflag\n";
std::fflush(NULL);
					}
				}
                        }
			else if(std::string((*i)->getStmtClassName()).find("BinaryOperator") != std::string::npos){
				if(((clang::BinaryOperator *)(*i))->isAssignmentOp()){
					if(((std::string)(((clang::Stmt *)((clang::Expr *)(((clang::BinaryOperator*)(*i))->getRHS()))->IgnoreCasts())->getStmtClassName())).find("CallExpr")!=std::string::npos){
				std::cout<<"BO: "<<((((((clang::CallExpr *)(((clang::Expr *)(((clang::BinaryOperator*)(*i))->getRHS()))->IgnoreCasts()))->getDirectCallee())->getNameInfo()).getName()).getAsString())<<"\n";
						if((((((((clang::CallExpr *)(((clang::Expr *)(((clang::BinaryOperator*)(*i))->getRHS()))->IgnoreCasts()))->getDirectCallee())->getNameInfo()).getName()).getAsString()).compare("malloc") == 0)||(((((((clang::CallExpr *)(((clang::Expr *)(((clang::BinaryOperator*)(*i))->getRHS()))->IgnoreCasts()))->getDirectCallee())->getNameInfo()).getName()).getAsString()).compare("realloc") == 0)||(((((((clang::CallExpr *)(((clang::Expr *)(((clang::BinaryOperator*)(*i))->getRHS()))->IgnoreCasts()))->getDirectCallee())->getNameInfo()).getName()).getAsString()).compare("calloc") == 0)){
							struct malloc_list m;
//std::cout<<"SC1 :"<<((clang::MemberExpr *)((clang::Stmt *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts())))->getMemberNameInfo().getName().getAsString()<<"\n";
//std::fflush(NULL);
//std::cout<<"SC2 :"<<((clang::DeclRefExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()))->getNameInfo().getName().getAsString()<<"\n";
//std::fflush(NULL);
                                                		//std::cout<<((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts())->getNameInfo().getName().getAsString()<<"\n";
                                                		//std::cout<<((clang::DeclRefExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getNameInfo().getName().getAsString()<<"\n";
							if(std::string((((clang::Stmt *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getStmtClassName())).find("ArraySubscriptExpr") != std::string::npos){
								m.name = (((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts())->getNameInfo().getName().getAsString());
								m.alloc_func = cur_func;
								m.alloc_scope = scope_now.name;
								m.alloc_file = scope_now.file;
								m.free_func = std::string("nofunc");
                                                                m.free_scope = std::string("noscope");
                                                                m.free_file = std::string("nofile");
								m.array_ref = 1;
								parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts())->getBeginLoc()), 0);
								parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts())->getEndLoc()), 1);
                                        		}
                                        		else if(std::string((((clang::Stmt *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getStmtClassName())).find("DeclRefExpr") != std::string::npos){
								m.name = ((clang::DeclRefExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getNameInfo().getName().getAsString();
								m.alloc_func = cur_func;
								m.alloc_scope = scope_now.name;
								m.alloc_file = scope_now.file;
								m.free_func = std::string("nofunc");
                                                                m.free_scope = std::string("noscope");
                                                                m.free_file = std::string("nofile");
								parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBeginLoc()), 0);
								parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getEndLoc()), 1);

                                        		}
							else if(std::string((((clang::Stmt *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getStmtClassName())).find("MemberExpr") != std::string::npos){
std::cout<<"SC :"<<((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getMemberNameInfo().getName().getAsString()<<"\n";
std::fflush(NULL);
std::cout<<"OPN :"<<std::string(((clang::Stmt *)((clang::MemberExpr *)(((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts())->getBase()->IgnoreCasts())->getStmtClassName())<<"\n";
std::fflush(NULL);
//add the code for struct and array of struct
std::cout<<"Hey\n";
std::fflush(NULL);
								if(std::string(((clang::Stmt *)((clang::MemberExpr *)(((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts())->getBase()->IgnoreCasts())->getStmtClassName()).find("ArraySubscriptExpr") != std::string::npos){
	                                                                m.name = (((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase())->IgnoreCasts())->getBase()->IgnoreCasts())->getNameInfo().getName().getAsString());
std::cout<<m.name<<"\n";
std::fflush(NULL);
									if(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->isArrow()){
										m.name = m.name + "->" + ((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getMemberNameInfo().getName().getAsString();
									}
									else{
										m.name = m.name + "." + ((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getMemberNameInfo().getName().getAsString();
									}
std::cout<<"Hi\n";
std::fflush(NULL);
        	                                                        m.alloc_func = cur_func;
                	                                                m.alloc_scope = scope_now.name;
                        	                                        m.alloc_file = scope_now.file;
                                	                                m.free_func = std::string("nofunc");
                                        	                        m.free_scope = std::string("noscope");
                                                	                m.free_file = std::string("nofile");
                                                        	        m.array_ref = 1;
                                                                	parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts()))->getBase()->IgnoreCasts())->getBeginLoc()), 0);
                                                                	parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)((clang::ArraySubscriptExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts()))->getBase()->IgnoreCasts())->getEndLoc()), 1);
                                                        	}
								else if(std::string(((clang::Stmt *)((clang::MemberExpr *)(((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts())->getBase()->IgnoreCasts())->getStmtClassName()).find("DeclRefExpr") != std::string::npos){
std::cout<<"HPP\n";
std::fflush(NULL);
                                                                	m.name = ((clang::DeclRefExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts()))->getNameInfo().getName().getAsString();
									if(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->isArrow()){
                                                                                m.name = m.name + std::string("->") + ((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getMemberNameInfo().getName().getAsString();
                                                                        }
                                                                        else{
                                                                                m.name = m.name + std::string(".") + ((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getMemberNameInfo().getName().getAsString();
                                                                        }
                                                                	m.alloc_func = cur_func;
                                                                	m.alloc_scope = scope_now.name;
                                                                	m.alloc_file = scope_now.file;
                                                                	m.free_func = std::string("nofunc");
                                                                	m.free_scope = std::string("noscope");
                                                                	m.free_file = std::string("nofile");
                                                                	parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts()))->getBeginLoc()), 0);
                                                                	parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts()))->getEndLoc()), 1);

                                                        	}
								if(std::string(((clang::Stmt *)((clang::MemberExpr *)(((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts())->getBase()->IgnoreCasts())->getStmtClassName()).find("CXXThisExpr") != std::string::npos){
								//	 m.name = ((clang::DeclRefExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts()))->getNameInfo().getName().getAsString();
                                                                   //     if(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->isArrow()){
                                                                                m.name = ((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getMemberNameInfo().getName().getAsString();
                                                                     //   }
                                                                       // else{
                                                                               // m.name = m.name + std::string(".") + ((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getMemberNameInfo().getName().getAsString();
                                                                       // }
									m.alloc_func = cur_func;
                                                                        m.alloc_scope = scope_now.name;
                                                                        m.alloc_file = scope_now.file;
                                                                        m.free_func = std::string("nofunc");
                                                                        m.free_scope = std::string("noscope");
                                                                        m.free_file = std::string("nofile");
                                                               //         parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts()))->getBeginLoc()), 0);
                                                                 //       parseloc_malloc(&m, getDeclLocation(((clang::DeclRefExpr *)(((clang::MemberExpr *)((((clang::BinaryOperator *)(*i))->getLHS())->IgnoreCasts()))->getBase()->IgnoreCasts()))->getEndLoc()), 1);

								}

							}
std::cout<<"AV: "<<m.name<<"\n";
std::fflush(NULL);
							alloc_list.push_back(m);
						}
					}
				}
			}
        		else if(std::string((*i)->getStmtClassName()).find("While") !=std::string::npos){
        			s.name = "while";
				s.func = cur_func;
				parseloc(&s, getDeclLocation((*i)->getBeginLoc()), 0);
 				parseloc(&s, getDeclLocation((*i)->getEndLoc()), 1);
        			scope_now.name = "while";
				scope_now.func = cur_func;
				parseloc(&scope_now, getDeclLocation((*i)->getBeginLoc()), 0);
 				parseloc(&scope_now, getDeclLocation((*i)->getEndLoc()), 1);
				//scope_eval(&s);	
				cur_scope.push_back(s);
  //              		llvm::outs() << "While Found " << " at "<< getDeclLocation((*i)->getBeginLoc()) << "ends at" << getDeclLocation((*i)->getEndLoc()) << "\n";
				clang::WhileStmt *w = ( clang::WhileStmt *)(*i);
				traverseBody(w->getBody());
                	}
                	else if(std::string((*i)->getStmtClassName()).find("For") !=std::string::npos){
        			s.name = "for";
				s.func = cur_func;
				parseloc(&s, getDeclLocation((*i)->getBeginLoc()), 0);
 				parseloc(&s, getDeclLocation((*i)->getEndLoc()), 1);
        			scope_now.name = "for";
				scope_now.func = cur_func;
				parseloc(&scope_now, getDeclLocation((*i)->getBeginLoc()), 0);
 				parseloc(&scope_now, getDeclLocation((*i)->getEndLoc()), 1);
				//scope_eval(&s);
				cur_scope.push_back(s);
    //            		llvm::outs() << "For Found " << " at "<< getDeclLocation((*i)->getBeginLoc()) << "ends at" << getDeclLocation((*i)->getEndLoc()) << "\n";
				clang::ForStmt *w = ( clang::ForStmt *)(*i);
				traverseBody(w->getBody());
                	}
                	else if(std::string((*i)->getStmtClassName()).find("If") !=std::string::npos){
        			s.name = "if";
				s.func = cur_func;
				parseloc(&s, getDeclLocation((*i)->getBeginLoc()), 0);
 				parseloc(&s, getDeclLocation((*i)->getEndLoc()), 1);
        			scope_now.name = "if";
				scope_now.func = cur_func;
				parseloc(&scope_now, getDeclLocation((*i)->getBeginLoc()), 0);
 				parseloc(&scope_now, getDeclLocation((*i)->getEndLoc()), 1);
				//scope_eval(&s);
				cur_scope.push_back(s);
      //                  	llvm::outs() << "IF Found " << " at "<< getDeclLocation((*i)->getBeginLoc()) << "ends at" << getDeclLocation((*i)->getEndLoc()) << "\n";
				clang::IfStmt *w = ( clang::IfStmt *)(*i);
				traverseBody(w->getThen());
				clang::IfStmt * is = (clang::IfStmt *)(w->getElse());
				while(std::string(is->getStmtClassName()).find("If") !=std::string::npos){
					struct scope si;
        				si.name = "elseif";
					si.func = cur_func;
					parseloc(&si, getDeclLocation((*is).getBeginLoc()), 0);
 					parseloc(&si, getDeclLocation((*is).getEndLoc()), 1);
        				scope_now.name = "elseif";
					scope_now.func = cur_func;
					parseloc(&scope_now, getDeclLocation((*is).getBeginLoc()), 0);
 					parseloc(&scope_now, getDeclLocation((*is).getEndLoc()), 1);
				//	scope_eval(&s);
					cur_scope.push_back(si);
	//				llvm::outs() << "Else IF Found " << " at "<< getDeclLocation(is->getBeginLoc()) << "ends at" << getDeclLocation(is->getEndLoc()) << "\n";
					traverseBody(((clang::IfStmt *)(w->getElse()))->getThen());
					is = ((clang::IfStmt *)(is->getElse()));
				}
				struct scope se;
        			se.name = "else";
				se.func = cur_func;
				parseloc(&se, getDeclLocation(((clang::Stmt *)is)->getBeginLoc()), 0);
 				parseloc(&se, getDeclLocation(((clang::Stmt *)is)->getEndLoc()), 1);
        			scope_now.name = "else";
				scope_now.func = cur_func;
				parseloc(&scope_now, getDeclLocation(((clang::Stmt *)is)->getBeginLoc()), 0);
 				parseloc(&scope_now, getDeclLocation(((clang::Stmt *)is)->getEndLoc()), 1);
				//scope_eval(&s);
				cur_scope.push_back(se);
	//			llvm::outs() << "Else Found " << " at "<< getDeclLocation(((clang::Stmt *)is)->getBeginLoc()) << "ends at" << getDeclLocation(((clang::Stmt *)is)->getEndLoc()) << "\n";
//llvm::outs()<<"D\n";
				traverseBody(is);
                	}
        	}
	return true;
  }

  bool VisitNamedDecl(clang::NamedDecl *NamedDecl) {
    std::string s = std::string(NamedDecl->getDeclKindName());
	std::cout<<"ND: "<<s<<"\n";
    if((s.find("Var") != std::string::npos)||(s.find("Field") != std::string::npos)){
	struct decl_scope_list v;
	v.name = NamedDecl->getQualifiedNameAsString();
	v.func = cur_func;
	if(s.find("Field") != std::string::npos){
		v.class_field = cur_class;
	}
	var_loc(&v, getDeclLocation(NamedDecl->getBeginLoc()));
	//v.scope = (cur_scope.top()).name;
	v.start_line = 0;
	v.start_column = 0;
	v.end_line = 9999999;
        v.end_column = 9999999;
        scope_eval(&v);
	var_list.push_front(v);
  //  	llvm::outs() << "Variable Found "<<NamedDecl->getDeclKindName()<<" "<<NamedDecl->getQualifiedNameAsString() << " at "<< getDeclLocation(NamedDecl->getBeginLoc()) << "\n";
    }
    else if((s.find("Function") != std::string::npos)||(s.find("Method") != std::string::npos)){
	if(NamedDecl->hasBody()){
		cur_func = NamedDecl->getQualifiedNameAsString();
		struct scope sc;
		sc.name = NamedDecl->getQualifiedNameAsString();
		sc.func = cur_func;
		if(s.find("Field") != std::string::npos){
                	sc.class_field = cur_class;
        	}
		parseloc(&sc, getDeclLocation(NamedDecl->getBeginLoc()), 0);
                parseloc(&sc, getDeclLocation(NamedDecl->getEndLoc()), 1);
		scope_now.name = NamedDecl->getQualifiedNameAsString();
		scope_now.func = cur_func;
		parseloc(&scope_now, getDeclLocation(NamedDecl->getBeginLoc()), 0);
                parseloc(&scope_now, getDeclLocation(NamedDecl->getEndLoc()), 1);
                //scope_eval(&sc);
                
		cur_scope.push_back(sc);
		clang::Stmt * stmts = NamedDecl->getBody();
		traverseBody(stmts);
	}
    //	llvm::outs() << "Function Found "<<NamedDecl->getDeclKindName()<<" "<<NamedDecl->getQualifiedNameAsString() << " at "<< getDeclLocation(NamedDecl->getBeginLoc()) << "\n";
    }
    else if(s.find("CXXRecord") != std::string::npos){
	if(NamedDecl->hasBody()){
                cur_class = NamedDecl->getQualifiedNameAsString();
	}
    }
    return true;
  }

private:
  std::string getDeclLocation(clang::SourceLocation Loc) const {
    std::ostringstream OSS;
    OSS << SourceManager.getFilename(Loc).str() << ":" << SourceManager.getSpellingLineNumber(Loc) << ":"<< SourceManager.getSpellingColumnNumber(Loc);
    return OSS.str();
  }
};

class DeclFinder : public clang::ASTConsumer {
  clang::SourceManager &SourceManager;
  DeclVisitor Visitor;
public:
  DeclFinder(clang::SourceManager &SM) : SourceManager(SM), Visitor(SM) {}

  void HandleTranslationUnit(clang::ASTContext &Context) final {
    auto Decls = Context.getTranslationUnitDecl()->decls();
    for (auto &Decl : Decls) {
      const auto& FileID = SourceManager.getFileID(Decl->getLocation());
      if (FileID != SourceManager.getMainFileID())
        continue;
      Visitor.TraverseDecl(Decl);
    }
  }
};

class DeclFindingAction : public clang::ASTFrontendAction {
public:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, clang::StringRef) final {
    return std::unique_ptr<clang::ASTConsumer>(new DeclFinder(CI.getSourceManager()));
  }
};

int main(int argc, const char **argv) {
    clang::tooling::CommonOptionsParser option(argc, argv, FindDeclCategory, FindDeclUsage);
    auto files = option.getSourcePathList();
    clang::tooling::ClangTool tool(option.getCompilations(), files);
    if(cur_func.compare("global") == 0){
	struct scope s;
	s.name = "global";
	s.start_line = 0;
	s.end_line = 9999999;
	s.start_column = 0;
        s.end_column = 9999999;
	scope_now.name = "global";
	scope_now.start_line = 0;
	scope_now.end_line = 9999999;
	scope_now.start_column = 0;
        scope_now.end_column = 9999999;
	cur_scope.push_back(s);
    }
    tool.run(clang::tooling::newFrontendActionFactory<DeclFindingAction>().get());
    eval_nesting();
    std::ofstream ofile;
    std::ofstream ofile1;
    std::ofstream ofile2;
    ofile.open("vardeclscope");
    for (std::list<struct decl_scope_list>::iterator it = var_list.begin(); it != var_list.end(); it++){
	ofile<<(*it).name<<" "<<(*it).file<<" "<<(*it).func<<" "<<(*it).scope<<" "<<(*it).decl_line<<" "<<(*it).decl_col<<" "<<(*it).start_line<<" "<<(*it).start_column<<" "<<(*it).end_line<<" "<<(*it).end_column<<"\n";
    }
    ofile.close();
    ofile1.open("nestingscore");
    for(std::list<struct scope>::iterator it = cur_scope.begin(); it != cur_scope.end(); it++){
	if((*it).name.compare("global")!=0){
		ofile1<<(*it).name<<" "<<(*it).file<<" "<<(*it).func<<" "<<(*it).start_line<<" "<<(*it).start_column<<" "<<(*it).end_line<<" "<<(*it).end_column<<" "<<(*it).nesting_score<<"\n";
	}
    }
    ofile1.close();
    ofile2.open("pointeralias");
    for(std::list<struct malloc_list>::iterator it = alloc_list.begin(); it != alloc_list.end(); it++){
        if((*it).name.compare("global")!=0){
                ofile2<<(*it).name<<" "<<(*it).array_ref<<" "<<(*it).alloc_file<<" "<<(*it).alloc_func<<" "<<(*it).alloc_scope<<" "<<(*it).alloc_line<<" "<<(*it).alloc_column<<" "<<(*it).free_file<<" "<<(*it).free_func<<" "<<(*it).free_scope<<" "<<(*it).free_line<<" "<<(*it).free_column<<"\n";
        }
    }
    ofile2.close();
    return 0;
}
