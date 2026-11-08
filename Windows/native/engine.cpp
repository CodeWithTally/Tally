// Tally native execution engine. Front-end AST -> compact indexed nodes -> synchronous execution.
#include "vendor/json.hpp"
#include <algorithm>
#include <chrono>
#include <charconv>
#include <filesystem>
#include <cmath>
#include <codecvt>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <memory>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
using json=nlohmann::json;using Text=std::u16string;
static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> utf;
static Text wide(const std::string&s){return utf.from_bytes(s);}static std::string narrow(const Text&s){Text clean;clean.reserve(s.size());for(size_t i=0;i<s.size();i++){char16_t c=s[i];if(c>=0xd800&&c<=0xdbff){if(i+1<s.size()&&s[i+1]>=0xdc00&&s[i+1]<=0xdfff){clean+=c;clean+=s[++i];}else clean+=0xfffd;}else clean+=(c>=0xdc00&&c<=0xdfff)?0xfffd:c;}return utf.to_bytes(clean);}
struct Error{int line;std::string message;};struct Stop{};static volatile std::sig_atomic_t interrupted=0;
#include "parser.hpp"
#include "unicode.hpp"
struct Value;using List=std::vector<Value>;
struct Scope;struct Function{int node=-1,builtin=-1;std::shared_ptr<Scope> scope;};
struct Value{enum Type{Null,Number,Boolean,String,Array,Func}type=Null;double num=0;std::shared_ptr<Text>str;std::shared_ptr<List>list;std::shared_ptr<Function>fn;
 Value()=default;Value(double x):type(Number),num(x){};static Value boolean(bool b){Value v;v.type=Boolean;v.num=b;return v;}static Value text(Text s){Value v;v.type=String;v.str=std::make_shared<Text>(std::move(s));return v;}static Value array(List a={}){Value v;v.type=Array;v.list=std::make_shared<List>(std::move(a));return v;}static Value function(int n,int b,std::shared_ptr<Scope>s){Value v;v.type=Func;v.fn=std::make_shared<Function>(Function{n,b,s});return v;}};
struct Scope{std::shared_ptr<Scope>parent;std::unordered_map<int,Value>values;explicit Scope(std::shared_ptr<Scope>p=nullptr):parent(p){values.reserve(8);}Value get(int id,int line,const std::string&name){auto it=values.find(id);if(it!=values.end())return it->second;if(parent)return parent->get(id,line,name);throw Error{line,"“"+name+"” has not been created yet. Try “set "+name+" to 0”."};}};
enum Kind{NUM,LITERAL,STR,LIST,VAR,UNARY,INDEX,BINARY,CALL,SET,SAY,ASK,CHANGE,APPEND,FUNCTION,IF,REPEAT,WHILE,FOR,RETURN,BREAK,CONTINUE,EXPRESSION,DRAW,WAIT,CLEAROUT,STOP};
struct Node{Kind kind;int line=1,name=-1,a=-1,b=-1,c=-1,op=0;double num=0;bool flag=false;Text text;std::vector<int>body,other,args,params;std::vector<std::pair<Text,int>>interpolated;};
static const std::vector<std::string>opNames={"+","-","*","/","//","%","**","==","!=","<",">","<=",">=","and","or","not","in","add","subtract","color","size","up","down","clear","right","left","goto","move","back"};
struct Segment{double x1,y1,x2,y2,size;std::string color;};
struct Control{int kind=0;Value value;};
struct Engine{
 std::chrono::steady_clock::time_point lastFlush=std::chrono::steady_clock::now();std::vector<Node>nodes;std::unordered_map<std::string,int>symbols;std::vector<std::string>names;std::vector<int>root;std::shared_ptr<Scope>global=std::make_shared<Scope>();std::vector<std::weak_ptr<Scope>>scopes;uint64_t steps=0,evals=0,limit=0;size_t depth=0,nextScopeSweep=4096;size_t output=0;double x=0,y=0,angle=0,size=3,dx=0,dy=1;bool pen=true,drawEnabled=true;std::string ink="#7055da",svgPath;std::vector<Segment>segments;std::ofstream trace;std::string pending;std::mt19937_64 rng{std::random_device{}()};
#ifdef _WIN32
 HANDLE drawPipe=nullptr;
#endif
 ~Engine(){flushDraw();
#ifdef _WIN32
 if(drawPipe)CloseHandle(drawPipe);
#endif
 // Break closure/scope ownership cycles at process completion.
 for(auto&w:scopes)if(auto s=w.lock())s->values.clear();global->values.clear();}
 [[noreturn]]void fail(std::string s,int l){throw Error{l,std::move(s)};}
 int symbol(const std::string&s){auto[it,newId]=symbols.emplace(s,(int)names.size());if(newId)names.push_back(s);return it->second;}
 int op(const std::string&s){auto it=std::find(opNames.begin(),opNames.end(),s);if(it==opNames.end())throw std::runtime_error("Unknown internal operator");return(int)(it-opNames.begin());}
 std::vector<int>block(const json&a,int line=1){std::vector<int>r;r.reserve(a.size());for(auto&n:a)r.push_back(load(n,line));return r;}
 int load(const json&j,int inherited=1){Node n;std::string k=j.at("kind");static const std::unordered_map<std::string,Kind>kinds={{"num",NUM},{"literal",LITERAL},{"str",STR},{"list",LIST},{"var",VAR},{"unary",UNARY},{"index",INDEX},{"binary",BINARY},{"call",CALL},{"set",SET},{"say",SAY},{"ask",ASK},{"change",CHANGE},{"append",APPEND},{"function",FUNCTION},{"if",IF},{"repeat",REPEAT},{"while",WHILE},{"for",FOR},{"return",RETURN},{"break",BREAK},{"continue",CONTINUE},{"expression",EXPRESSION},{"draw",DRAW},{"wait",WAIT},{"clearOutput",CLEAROUT},{"stop",STOP}};n.kind=kinds.at(k);n.line=j.value("line",inherited);if(j.contains("name"))n.name=symbol(j.at("name"));if(j.contains("op"))n.op=op(j.at("op"));
 auto ex=[&](const char*s){return load(j.at(s),n.line);};
 switch(n.kind){case NUM:n.num=j.at("value");break;case LITERAL:n.flag=!j.at("value").is_null();n.num=n.flag?j.at("value").get<bool>():0;break;case STR:{n.text=wide(j.at("value"));std::string s=j.at("value");size_t start=0,p=0;while((p=s.find('{',p))!=std::string::npos){size_t e=s.find('}',p+1);if(e==std::string::npos)break;std::string name=s.substr(p+1,e-p-1);bool valid=!name.empty()&&(std::isalpha((unsigned char)name[0])||name[0]=='_');for(char c:name)valid=valid&&(std::isalnum((unsigned char)c)||c=='_');if(valid){n.interpolated.push_back({wide(s.substr(start,p-start)),symbol(name)});start=e+1;p=e+1;}else p++;}if(!n.interpolated.empty())n.interpolated.push_back({wide(s.substr(start)),-1});break;}
 case LIST:n.args=block(j.at("values"),n.line);break;case UNARY:n.a=ex("value");break;case INDEX:n.a=ex("object");n.b=ex("index");break;case BINARY:n.a=ex("left");n.b=ex("right");break;case CALL:n.a=ex("callee");n.args=block(j.at("args"),n.line);break;
 case SET:case SAY:case CHANGE:case APPEND:case RETURN:case EXPRESSION:case WAIT:n.a=ex("value");if(n.kind==WAIT)n.num=j.at("scale");break;
 case ASK:n.a=ex("prompt");n.flag=j.at("number");break;case FUNCTION:for(auto&p:j.at("params"))n.params.push_back(symbol(p));n.body=block(j.at("body"),n.line);break;
 case IF:n.a=ex("test");n.body=block(j.at("yes"),n.line);n.other=block(j.at("no"),n.line);break;
 case REPEAT:case WHILE:case FOR:n.a=ex(n.kind==REPEAT?"count":n.kind==WHILE?"test":"items");n.body=block(j.at("body"),n.line);break;
 case DRAW:if(j.contains("value"))n.a=ex("value");if(j.contains("y"))n.b=ex("y");break;default:break;}
 int id=nodes.size();nodes.push_back(std::move(n));return id;
 }
 void check(int l){if(interrupted)fail("Program stopped.",l);}void tick(int l){if(++steps&&limit&&steps>limit)fail("Instruction limit reached. Use --max-steps to increase the desktop limit.",l);check(l);if((steps&1023)==0){auto now=std::chrono::steady_clock::now();if(now-lastFlush>=std::chrono::milliseconds(16)){flushDraw();std::cout.flush();lastFlush=now;}}}
 double number(const Value&v,int l){if(v.type!=Value::Number||!std::isfinite(v.num))fail("This instruction needs a finite number.",l);return v.num;}bool boolean(const Value&v,int l){if(v.type!=Value::Boolean)fail("A condition must be true or false.",l);return v.num!=0;}Value finite(double d,int l){if(!std::isfinite(d))fail("This instruction needs a finite number.",l);return Value(d);}
 std::string show(const Value&v,int nesting=0){switch(v.type){case Value::Null:return"nothing";case Value::Boolean:return v.num?"true":"false";case Value::String:return narrow(*v.str);case Value::Number:{if(v.num==0)return"0";char buf[128];auto result=std::to_chars(buf,buf+sizeof(buf),v.num,std::chars_format::general);std::string out(buf,result.ptr);auto ep=out.find('e');if(ep==std::string::npos)return out;int exp=std::stoi(out.substr(ep+1));std::string mant=out.substr(0,ep),sign;if(mant[0]=='-'){sign="-";mant.erase(0,1);}auto dot=mant.find('.');int decimal=dot==std::string::npos?(int)mant.size():(int)dot;if(dot!=std::string::npos)mant.erase(dot,1);if(exp>=-6&&exp<21){int pos=decimal+exp;if(pos<=0)return sign+"0."+std::string(-pos,'0')+mant;if(pos>=(int)mant.size())return sign+mant+std::string(pos-mant.size(),'0');mant.insert(pos,".");return sign+mant;}return sign+out.substr(sign.size(),ep-sign.size())+"e"+(exp>=0?"+":"-")+std::to_string(std::abs(exp));}case Value::Func:return"[function]";case Value::Array:{std::string s="[";for(size_t i=0;i<v.list->size();i++){if(i)s+=", ";s+=show((*v.list)[i],nesting+1);}return s+"]";}}return"";}
 bool equal(const Value&a,const Value&b,int nesting=0){if(a.type!=b.type)return false;switch(a.type){case Value::Null:return true;case Value::Number:case Value::Boolean:return a.num==b.num;case Value::String:return*a.str==*b.str;case Value::Func:return a.fn==b.fn;case Value::Array:if(a.list->size()!=b.list->size())return false;for(size_t i=0;i<a.list->size();i++)if(!equal((*a.list)[i],(*b.list)[i],nesting+1))return false;return true;}return false;}
 Value get(std::shared_ptr<Scope>s,int id,int l){return s->get(id,l,names.at(id));}
 static bool integer(double d){return std::isfinite(d)&&std::floor(d)==d&&std::abs(d)<=9007199254740991.0;}

 template<size_t N>static bool inRanges(uint32_t cp,const uint32_t(&ranges)[N][2]){size_t lo=0,hi=N;while(lo<hi){size_t mid=(lo+hi)/2;if(cp<ranges[mid][0])hi=mid;else if(cp>ranges[mid][1])lo=mid+1;else return true;}return false;}
 static void addCodePoint(Text&out,uint32_t cp){if(cp<=0xffff)out+=(char16_t)cp;else{cp-=0x10000;out+=(char16_t)(0xd800+(cp>>10));out+=(char16_t)(0xdc00+(cp&1023));}}
 Text changeCase(const Text&input,bool upper){std::vector<uint32_t>points;points.reserve(input.size());for(size_t i=0;i<input.size();i++){uint32_t cp=input[i];if(cp>=0xd800&&cp<=0xdbff&&i+1<input.size()&&input[i+1]>=0xdc00&&input[i+1]<=0xdfff){cp=0x10000+((cp-0xd800)<<10)+(input[++i]-0xdc00);}points.push_back(cp);}Text out;out.reserve(input.size());for(size_t i=0;i<points.size();i++){uint32_t cp=points[i];if(!upper&&cp==0x3a3){bool before=false,after=false;for(size_t j=i;j>0;){uint32_t p=points[--j];if(inRanges(p,ignoredRanges))continue;before=inRanges(p,casedRanges);break;}for(size_t j=i+1;j<points.size();j++){uint32_t p=points[j];if(inRanges(p,ignoredRanges))continue;after=inRanges(p,casedRanges);break;}if(before&&!after){out+=(char16_t)0x3c2;continue;}}
 auto entry=std::lower_bound(std::begin(caseEntries),std::end(caseEntries),cp,[](const CaseEntry&e,uint32_t p){return e.cp<p;});if(entry!=std::end(caseEntries)&&entry->cp==cp)out.append(caseData+(upper?entry->u:entry->l),upper?entry->un:entry->ln);else addCodePoint(out,cp);}return out;}
 Value builtin(int id,const List&a,int l){static int mins[]={1,1,1,1,1,1,1,1,2,1,2,1,1};static int maxs[]={1,1,1,1,1,1,1,1,2,3,2,1,1};if(a.size()<(size_t)mins[id]||a.size()>(size_t)maxs[id])fail("This function received the wrong number of arguments.",l);auto n=[&](size_t i){return number(a.at(i),l);};switch(id){case 0:if(a[0].type==Value::Array)return Value((double)a[0].list->size());if(a[0].type==Value::String)return Value((double)a[0].str->size());fail("length() needs a list or text.",l);
 case 1:{if(a[0].type==Value::Number)return a[0];if(a[0].type!=Value::String)fail("That value is not a number.",l);
 auto space=[](char16_t c){return(c>=9&&c<=13)||c==32||c==0xa0||c==0x1680||(c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||c==0x202f||c==0x205f||c==0x3000||c==0xfeff;};
 const Text&t=*a[0].str;size_t start=0,end=t.size();while(start<end&&space(t[start]))start++;while(end>start&&space(t[end-1]))end--;if(start==end)fail("That value is not a number.",l);std::string text=narrow(t.substr(start,end-start));
 if(text.size()>2&&text[0]=='0'&&(text[1]=='x'||text[1]=='X'||text[1]=='b'||text[1]=='B'||text[1]=='o'||text[1]=='O')){int base=(text[1]=='x'||text[1]=='X')?16:(text[1]=='b'||text[1]=='B')?2:8;double d=0;for(size_t i=2;i<text.size();i++){char c=text[i];int v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:99;if(v>=base)fail("That value is not a number.",l);d=d*base+v;}return finite(d,l);}
 size_t i=0;if(text[i]=='+'||text[i]=='-')i++;bool digits=false;while(i<text.size()&&text[i]>='0'&&text[i]<='9'){digits=true;i++;}if(i<text.size()&&text[i]=='.'){i++;while(i<text.size()&&text[i]>='0'&&text[i]<='9'){digits=true;i++;}}if(!digits)fail("That value is not a number.",l);if(i<text.size()&&(text[i]=='e'||text[i]=='E')){i++;if(i<text.size()&&(text[i]=='+'||text[i]=='-'))i++;size_t exp=i;while(i<text.size()&&text[i]>='0'&&text[i]<='9')i++;if(i==exp)fail("That value is not a number.",l);}if(i!=text.size())fail("That value is not a number.",l);return finite(std::strtod(text.c_str(),nullptr),l);}
 case 2:return Value::text(wide(show(a[0])));case 3:{double v=n(0),f=std::floor(v);return finite(v-f<0.5?f:f+1,l);}case 4:return finite(std::floor(n(0)),l);case 5:return finite(std::ceil(n(0)),l);case 6:return finite(std::abs(n(0)),l);case 7:return finite(std::sqrt(n(0)),l);
 case 8:{double lo=n(0),hi=n(1);if(!integer(lo)||!integer(hi)||hi<lo||!integer(hi-lo+1))fail("random() needs two whole numbers, lowest first, within a safe range.",l);return Value((double)std::uniform_int_distribution<int64_t>((int64_t)lo,(int64_t)hi)(rng));}
 case 9:{double start=a.size()==1?0:n(0),end=a.size()==1?n(0):n(1),step=a.size()==3?n(2):1;if(!integer(start)||!integer(end)||!integer(step)||step==0)fail("range() needs whole numbers and a nonzero step.",l);double count=std::max(0.0,std::ceil((end-start)/step));List out;if(count>(double)out.max_size())fail("This range cannot fit in addressable memory.",l);out.reserve((size_t)count);for(size_t i=0;i<(size_t)count;i++)out.emplace_back(start+i*step);return Value::array(std::move(out));}
 case 11:case 12:if(a[0].type!=Value::String)fail("This function needs text.",l);return Value::text(changeCase(*a[0].str,id==11));
 case 10:if(a[0].type==Value::Array){for(auto&v:*a[0].list)if(equal(v,a[1]))return Value::boolean(true);return Value::boolean(false);}if(a[0].type==Value::String&&a[1].type==Value::String)return Value::boolean(a[0].str->find(*a[1].str)!=Text::npos);fail("contains() needs a list or two pieces of text.",l);}
 return{};
 }
 Value eval(int id,std::shared_ptr<Scope>s){const Node&n=nodes[id];int l=n.line;++evals;check(l);switch(n.kind){case NUM:return Value(n.num);case LITERAL:return n.flag?Value::boolean(n.num):Value();case STR:if(n.interpolated.empty())return Value::text(n.text);else{Text t;for(auto&part:n.interpolated){t+=part.first;if(part.second>=0)t+=wide(show(get(s,part.second,l)));}return Value::text(std::move(t));}case VAR:return get(s,n.name,l);case LIST:{List a;a.reserve(n.args.size());for(int i:n.args)a.push_back(eval(i,s));return Value::array(std::move(a));}
 case UNARY:{Value a=eval(n.a,s);return n.op==15?Value::boolean(!boolean(a,l)):finite(n.op==1?-number(a,l):number(a,l),l);}
 case INDEX:{Value a=eval(n.a,s),b=eval(n.b,s);double i=number(b,l);size_t count=a.type==Value::Array?a.list->size():a.type==Value::String?a.str->size():0;if(a.type!=Value::Array&&a.type!=Value::String)fail("Only lists and text can be indexed.",l);if(!integer(i)||i<0||i>=count)fail("Index is out of range. Indexes start at 0.",l);return a.type==Value::Array?(*a.list)[(size_t)i]:Value::text(a.str->substr((size_t)i,1));}
 case BINARY:{Value a=eval(n.a,s);if(n.op==13)return Value::boolean(boolean(a,l)&&boolean(eval(n.b,s),l));if(n.op==14)return Value::boolean(boolean(a,l)||boolean(eval(n.b,s),l));Value b=eval(n.b,s);if(n.op==7||n.op==8)return Value::boolean(equal(a,b)==(n.op==7));if(n.op==16){if(b.type==Value::Array){for(auto&v:*b.list)if(equal(a,v))return Value::boolean(true);return Value::boolean(false);}if(a.type==Value::String&&b.type==Value::String)return Value::boolean(b.str->find(*a.str)!=Text::npos);fail("Use in with a list, or text inside text.",l);}if(n.op>=9&&n.op<=12){if(a.type!=b.type||(a.type!=Value::Number&&a.type!=Value::String))fail("Compare two numbers or two pieces of text.",l);int cmp=a.type==Value::String?a.str->compare(*b.str):(a.num<b.num?-1:a.num>b.num?1:0);return Value::boolean(n.op==9?cmp<0:n.op==10?cmp>0:n.op==11?cmp<=0:cmp>=0);}if(n.op==0&&a.type==Value::String&&b.type==Value::String){return Value::text(*a.str+*b.str);}double x=number(a,l),y=number(b,l);if((n.op==3||n.op==4||n.op==5)&&y==0)fail("You cannot divide by zero.",l);return finite(n.op==0?x+y:n.op==1?x-y:n.op==2?x*y:n.op==3?x/y:n.op==4?std::floor(x/y):n.op==5?std::fmod(std::fmod(x,y)+y,y):std::pow(x,y),l);}
 case CALL:{Value f=eval(n.a,s);if(f.type!=Value::Func)fail("This value is not a function.",l);List args;args.reserve(n.args.size());for(int i:n.args)args.push_back(eval(i,s));if(f.fn->builtin>=0)return builtin(f.fn->builtin,args,l);const Node&def=nodes[f.fn->node];if(args.size()!=def.params.size())fail("This function received the wrong number of arguments.",l);auto local=std::make_shared<Scope>(f.fn->scope);scopes.emplace_back(local);if(scopes.size()>nextScopeSweep){scopes.erase(std::remove_if(scopes.begin(),scopes.end(),[](auto&w){return w.expired();}),scopes.end());nextScopeSweep=std::max(size_t(4096),scopes.size()*2);}for(size_t i=0;i<args.size();i++)local->values[def.params[i]]=args[i];depth++;Control r;try{r=execute(def.body,local);}catch(...){depth--;throw;}depth--;return r.kind==1?r.value:Value();}default:fail("Invalid expression node.",l);}}
 bool wantsProtocol(){
#ifdef _WIN32
return drawPipe||trace.is_open();
#else
return trace.is_open();
#endif
}
void appendNumber(double n){char b[64];auto r=std::to_chars(b,b+sizeof(b),n,std::chars_format::general);pending.append(b,r.ptr);}
void flushDraw(){if(cursorDirty&&wantsProtocol()){std::ostringstream out;out<<std::setprecision(17)<<"P "<<x<<' '<<y<<' '<<angle<<' '<<(pen?1:0)<<' '<<hexColor(ink)<<'\n';pending+=out.str();cursorDirty=false;}if(pending.empty())return;if(trace)trace<<pending;
#ifdef _WIN32
 if(drawPipe){DWORD written=0;if(!WriteFile(drawPipe,pending.data(),(DWORD)pending.size(),&written,NULL)){CloseHandle(drawPipe);drawPipe=nullptr;}}
#endif
 pending.clear();}
 void ensureDrawing(){
#ifdef _WIN32
 if(!drawEnabled||drawingStarted)return;
 drawingStarted=true;
 wchar_t exe[32768];GetModuleFileNameW(NULL,exe,32768);
 wchar_t*slash=wcsrchr(exe,L'\\');if(!slash)return;wcscpy(slash+1,L"tally-draw.exe");
 HANDLE rd,wr,owner=nullptr;SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};
 if(!CreatePipe(&rd,&wr,&sa,65536))return;
 SetHandleInformation(wr,HANDLE_FLAG_INHERIT,0);
 // The child owns a real handle to this process, not a reusable process ID.
 if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&owner,SYNCHRONIZE,TRUE,0)){CloseHandle(rd);CloseHandle(wr);return;}
 STARTUPINFOW si={};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;
 si.hStdInput=rd;si.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
 PROCESS_INFORMATION pi={};wchar_t cmd[32768];
 swprintf(cmd,32768,L"\"%ls\" --parent-handle %llu",exe,(unsigned long long)(uintptr_t)owner);
 if(CreateProcessW(exe,cmd,NULL,NULL,TRUE,0,NULL,NULL,&si,&pi)){drawPipe=wr;CloseHandle(pi.hProcess);CloseHandle(pi.hThread);}else CloseHandle(wr);
 CloseHandle(owner);CloseHandle(rd);
#endif
 }
 bool drawingStarted=false,cursorDirty=false;
 std::string hexColor(const std::string&c){static const std::unordered_map<std::string,std::string>m={{"black","000000"},{"white","ffffff"},{"red","ff0000"},{"blue","0000ff"},{"green","008000"},{"yellow","ffff00"},{"orange","ffa500"},{"purple","800080"},{"pink","ffc0cb"},{"teal","008080"},{"navy","000080"},{"gray","808080"},{"grey","808080"},{"brown","a52a2a"},{"cyan","00ffff"},{"magenta","ff00ff"}};if(c.size()&&(c[0]=='#')){std::string v=c.substr(1);if(v.size()!=3&&v.size()!=6)return"";for(char ch:v)if(!std::isxdigit((unsigned char)ch))return"";if(v.size()==3){std::string s;for(char ch:v){s+=ch;s+=ch;}return s;}return v;}auto it=m.find(c);return it==m.end()?"":it->second;}
 void draw(const Node&n,std::shared_ptr<Scope>s){int l=n.line;Value v=n.a>=0?eval(n.a,s):Value();ensureDrawing();cursorDirty=true;if(n.op==19){if(v.type!=Value::String)fail("Use a valid pen color.",l);std::string c=narrow(*v.str);if(hexColor(c).empty())fail("Use a valid pen color.",l);ink=c;}else if(n.op==20){double z=number(v,l);if(z<=0)fail("Pen size must be greater than 0.",l);size=z;}else if(n.op==21||n.op==22)pen=n.op==22;else if(n.op==23){segments.clear();pending+="C\n";}else if(n.op==24||n.op==25){angle=std::fmod(angle+number(v,l)*(n.op==24?1:-1),360);dx=std::sin(angle*3.141592653589793/180);dy=std::cos(angle*3.141592653589793/180);}else{double ox=x,oy=y;if(n.op==26){x=number(v,l);y=number(eval(n.b,s),l);}else{double distance=number(v,l)*(n.op==28?-1:1);x+=dx*distance;y+=dy*distance;}if(!std::isfinite(x)||!std::isfinite(y))fail("Drawing coordinates must be finite numbers.",l);if(pen){segments.push_back({ox,oy,x,y,size,ink});if(wantsProtocol()){pending+="L ";appendNumber(ox);pending+=' ';appendNumber(oy);pending+=' ';appendNumber(x);pending+=' ';appendNumber(y);pending+=' ';pending+=hexColor(ink);pending+=' ';appendNumber(size);pending+='\n';}}}if(pending.size()>32768)flushDraw();}
 bool reaches(const Value&v,const std::shared_ptr<List>&target,std::unordered_set<const List*>&seen){if(v.type!=Value::Array)return false;if(v.list==target)return true;if(!seen.insert(v.list.get()).second)return false;for(auto&x:*v.list)if(reaches(x,target,seen))return true;return false;}
 Control execute(const std::vector<int>&body,std::shared_ptr<Scope>s){for(int id:body){const Node&n=nodes[id];int l=n.line;tick(l);auto ev=[&](){return eval(n.a,s);};Control r;switch(n.kind){case SET:s->values[n.name]=ev();break;case SAY:{std::string str=show(ev());output+=wide(str).size()+1;std::cout<<str<<'\n';break;}case ASK:{flushDraw();std::string p=show(ev());
#ifdef _WIN32
if(_isatty(_fileno(stdin)))std::cout<<p<<' '<<std::flush;
#else
if(isatty(fileno(stdin)))std::cout<<p<<' '<<std::flush;
#endif
std::string input;if(!std::getline(std::cin,input))fail("Input was cancelled.",l);if(!input.empty()&&input.back()=='\r')input.pop_back();s->values[n.name]=n.flag?builtin(1,{Value::text(wide(input))},l):Value::text(wide(input));break;}case CHANGE:{double a=number(get(s,n.name,l),l),b=number(ev(),l);s->values[n.name]=finite(a+(n.op==17?b:-b),l);break;}case APPEND:{Value a=get(s,n.name,l);if(a.type!=Value::Array)fail("append needs a list.",l);Value v=ev();std::unordered_set<const List*>seen;if(reaches(v,a.list,seen))fail("A list cannot contain itself.",l);a.list->push_back(v);break;}case FUNCTION:s->values[n.name]=Value::function(id,-1,s);break;case IF:r=execute(boolean(ev(),l)?n.body:n.other,s);break;case REPEAT:{double count=number(ev(),l);if(!integer(count)||count<0)fail("Repeat needs a whole number that is 0 or greater.",l);for(uint64_t i=0;i<count;i++){if(i)tick(l);auto c=execute(n.body,s);if(c.kind==1){r=c;break;}if(c.kind==2)break;}break;}case WHILE:while(boolean(ev(),l)){auto c=execute(n.body,s);if(c.kind==1){r=c;break;}if(c.kind==2)break;tick(l);}break;case FOR:{Value a=ev();List copy;if(a.type==Value::Array)copy=*a.list;else if(a.type==Value::String){for(size_t i=0;i<a.str->size();i++){size_t len=((*a.str)[i]>=0xd800&&(*a.str)[i]<=0xdbff&&i+1<a.str->size()&&(*a.str)[i+1]>=0xdc00&&(*a.str)[i+1]<=0xdfff)?2:1;copy.push_back(Value::text(a.str->substr(i,len)));i+=len-1;}}else fail("for each needs a list or text.",l);for(auto&item:copy){s->values[n.name]=item;tick(l);auto c=execute(n.body,s);if(c.kind==1){r=c;break;}if(c.kind==2)break;}break;}case RETURN:return{1,ev()};case BREAK:return{2,{}};case CONTINUE:return{3,{}};case EXPRESSION:ev();break;case DRAW:draw(n,s);break;case WAIT:{double ms=number(ev(),l)*n.num;if(!std::isfinite(ms)||ms<0)fail("Wait needs a finite, nonnegative duration.",l);flushDraw();std::cout.flush();while(ms>0){check(l);auto start=std::chrono::steady_clock::now();std::this_thread::sleep_for(std::chrono::duration<double,std::milli>(std::min(ms,20.0)));ms-=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}check(l);break;}case CLEAROUT:output=0;
#ifdef _WIN32
 if(_isatty(_fileno(stdout)))std::cout<<"\033[2J\033[H";
#else
 if(isatty(fileno(stdout)))std::cout<<"\033[2J\033[H";
#endif
 break;case STOP:throw Stop{};default:fail("Invalid statement node.",l);}if(r.kind)return r;}return{};}
 void init(const json&ast){nodes.reserve(4096);root=block(ast);const char*bs[]={"length","number","text","round","floor","ceil","abs","sqrt","random","range","contains","upper","lower"};for(int i=0;i<13;i++)global->values[symbol(bs[i])]=Value::function(-1,i,nullptr);segments.reserve(16384);}
 void finish(){std::ostringstream p;p<<std::setprecision(17)<<"P "<<x<<' '<<y<<' '<<angle<<' '<<(pen?1:0)<<' '<<hexColor(ink)<<'\n';pending+=p.str();flushDraw();if(!svgPath.empty()){std::ofstream o(std::filesystem::u8path(svgPath));if(!o)throw std::runtime_error("Cannot save SVG");o<<std::setprecision(17)<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\" viewBox=\"-400 -300 800 600\"><rect x=\"-400\" y=\"-300\" width=\"800\" height=\"600\" fill=\"#faf9f6\"/>";for(auto&s:segments)o<<"<line x1=\""<<s.x1<<"\" y1=\""<<-s.y1<<"\" x2=\""<<s.x2<<"\" y2=\""<<-s.y2<<"\" stroke=\""<<s.color<<"\" stroke-width=\""<<s.size<<"\" stroke-linecap=\"round\"/>";o<<"</svg>";}}
};
int tallyMain(int argc,char**argv){
 std::signal(SIGINT,[](int){interrupted=1;});std::ios::sync_with_stdio(false);bool pause=false;
#ifdef _WIN32
 DWORD pids[4];pause=GetConsoleProcessList(pids,4)<=1;
#endif
 int code=0;
 try{
  Engine e;bool stats=false,astInput=false,help=false,version=false;std::string program,dumpPath;
  for(int i=1;i<argc;i++){std::string a=argv[i];auto value=[&](){if(i+1>=argc)throw std::runtime_error("Provide a value after "+a+".");return std::string(argv[++i]);};
   if(a=="--no-drawing")e.drawEnabled=false;else if(a=="--svg")e.svgPath=value();else if(a=="--trace")e.trace.open(std::filesystem::u8path(value()));else if(a=="--stats")stats=true;else if(a=="--pause")pause=true;else if(a=="--help")help=true;else if(a=="--version")version=true;else if(a=="--ast")astInput=true;else if(a=="--dump-ast")dumpPath=value();else if(a=="--max-steps"){std::string v=value();if(v.empty()||v.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("max-steps needs a nonnegative whole number; 0 means unlimited.");e.limit=std::stoull(v);}else if(a.rfind("--",0)==0)throw std::runtime_error("Unknown option: "+a);else if(!program.empty())throw std::runtime_error("Provide one .tally file at a time.");else program=a;
  }
  if(version)std::cout<<"Tally 0.4.1 (native C++)\n";
  else if(help||program.empty())std::cout<<"Tally 0.4.1 — standalone native runner\n\nRun: tally program.tally\n     tally program.tally --svg drawing.svg\n\nOptions: --pause --no-drawing --stats --max-steps N --version --help\n\nNo installed runtime required. No fixed code or drawing caps.\nCtrl+C stops a running program. --max-steps is optional; 0 means unlimited.\n";
  else{
   std::ifstream in(std::filesystem::u8path(program),std::ios::binary);if(!in)throw std::runtime_error("Cannot read program: "+program);json ast;
   auto start=std::chrono::steady_clock::now();if(astInput)in>>ast;else{std::string source((std::istreambuf_iterator<char>(in)),{});ast=SourceParser(std::move(source)).parse();}
   if(!dumpPath.empty()){std::ofstream out(std::filesystem::u8path(dumpPath));if(!out)throw std::runtime_error("Cannot save parser output");out<<ast.dump();}
   else{e.init(ast);ast=json();auto parsed=std::chrono::steady_clock::now();try{e.execute(e.root,e.global);}catch(const Stop&){}e.finish();if(stats)std::cerr<<"Native execution: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-parsed).count()<<" ms; parse/load: "<<std::chrono::duration<double,std::milli>(parsed-start).count()<<" ms; "<<e.steps<<" instructions; "<<e.segments.size()<<" segments\n";}
  }
 }catch(const Error&e){std::cerr<<"Line "<<e.line<<": "<<e.message<<'\n';code=1;}catch(const std::bad_alloc&){std::cerr<<"Tally: Not enough memory to continue.\n";code=1;}catch(const std::exception&e){std::cerr<<"Tally: "<<e.what()<<'\n';code=1;}
#ifdef _WIN32
 bool terminal=_isatty(_fileno(stdin));
#else
 bool terminal=isatty(fileno(stdin));
#endif
 if(pause&&terminal){std::cout<<(code?"\nProgram ended with an error.":"\nProgram finished.")<<"\nPress Enter to close this window…"<<std::flush;std::cin.clear();std::string line;std::getline(std::cin,line);}return code;
}

#ifdef _WIN32
static void brandConsole(){
 SetConsoleTitleW(L"Tally");
 HWND console=GetConsoleWindow();if(!console)return;
 for(int small=0;small<2;small++){
  HICON icon=(HICON)LoadImageW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(small?SM_CXSMICON:SM_CXICON),GetSystemMetrics(small?SM_CYSMICON:SM_CYICON),LR_SHARED);
  if(icon){DWORD_PTR result;SendMessageTimeoutW(console,WM_SETICON,small?ICON_SMALL:ICON_BIG,(LPARAM)icon,SMTO_ABORTIFHUNG,250,&result);}
 }
}
int wmain(int argc,wchar_t**argv){brandConsole();SetConsoleCP(CP_UTF8);SetConsoleOutputCP(CP_UTF8);DWORD mode;HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);if(GetConsoleMode(h,&mode))SetConsoleMode(h,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);std::vector<std::string>args;std::vector<char*>ptrs;for(int i=0;i<argc;i++){int n=WideCharToMultiByte(CP_UTF8,0,argv[i],-1,nullptr,0,nullptr,nullptr);std::string s(n,'\0');WideCharToMultiByte(CP_UTF8,0,argv[i],-1,s.data(),n,nullptr,nullptr);s.pop_back();args.push_back(s);}for(auto&a:args)ptrs.push_back(a.data());return tallyMain(argc,ptrs.data());}
#else
int main(int argc,char**argv){return tallyMain(argc,argv);}
#endif
