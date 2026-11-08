// Native Tally lexer, Pratt expression parser, and indentation parser.
#pragma once
struct Token { std::string type, text; double number=0; };
using Tokens=std::vector<Token>;
static bool identifierStart(unsigned char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';}
static bool digit(unsigned char c){return c>='0'&&c<='9';}
static bool matches(const Tokens&t,size_t i,const std::string&phrase){std::istringstream words(phrase);std::string word;while(words>>word){if(i>=t.size()||t[i].type=="str"||t[i].text!=word)return false;i++;}return true;}
static Tokens tokenize(const std::string&source,int line){
 Tokens out;size_t i=0;
 while(i<source.size()){
  unsigned char c=source[i];if(std::isspace(c)){i++;continue;}if(c=='#')break;
  if(c=='"'||c=='\''){
   char quote=c;std::string value;i++;bool closed=false;
   while(i<source.size()){char ch=source[i++];if(ch==quote){closed=true;break;}if(ch=='\\'){if(i>=source.size())break;char e=source[i++];ch=e=='n'?'\n':e=='r'?'\r':e=='t'?'\t':e;}value+=ch;}
   if(!closed)throw Error{line,"This text is missing its closing quote."};out.push_back({"str",value});continue;
  }
  if(digit(c)||(c=='.'&&i+1<source.size()&&digit(source[i+1]))){size_t start=i;while(i<source.size()&&digit(source[i]))i++;if(i<source.size()&&source[i]=='.'){i++;while(i<source.size()&&digit(source[i]))i++;}std::string n=source.substr(start,i-start);double value=std::strtod(n.c_str(),nullptr);if(!std::isfinite(value))throw Error{line,"This instruction needs a finite number."};out.push_back({"num",n,value});continue;}
  if(identifierStart(c)){size_t start=i++;while(i<source.size()&&(identifierStart(source[i])||digit(source[i])))i++;out.push_back({"id",source.substr(start,i-start)});continue;}
  std::string two=source.substr(i,2);if(two=="=="||two=="!="||two=="<="||two==">="||two=="**"||two=="//"){out.push_back({"op",two});i+=2;continue;}
  if(std::string("+-*/%<>()[],:=").find(c)!=std::string::npos){out.push_back({"op",std::string(1,c)});i++;continue;}
  throw Error{line,"I don't recognize “"+std::string(1,c)+"”."};
 }
 return out;
}
struct ExpressionParser {
 const Tokens&t;size_t i,end;int line;
 ExpressionParser(const Tokens&tokens,int l,size_t start,size_t stop):t(tokens),i(start),end(stop),line(l){}
 bool at(const std::string&s){size_t words=1+std::count(s.begin(),s.end(),' ');return i+words<=end&&matches(t,i,s);}
 bool take(const std::string&s){if(!at(s))return false;i+=1+std::count(s.begin(),s.end(),' ');return true;}
 void need(const std::string&s){if(!take(s))throw Error{line,"Expected “"+s+"” here."};}
 json parse(int min=0){
  if(i>=end)throw Error{line,"An expression is missing here."};Token token=t[i++];json left;
  if(token.type=="num")left={{"kind","num"},{"value",token.number}};
  else if(token.type=="str")left={{"kind","str"},{"value",token.text}};
  else if(token.text=="("){left=parse();need(")");}
  else if(token.text=="["){json values=json::array();if(!take("]")){do{values.push_back(parse());}while(take(","));need("]");}left={{"kind","list"},{"values",std::move(values)}};}
  else if(token.text=="-"||token.text=="+"||token.text=="not")left={{"kind","unary"},{"op",token.text},{"value",parse(token.text=="not"?25:65)}};
  else if(token.type=="id"){if(token.text=="true"||token.text=="false"||token.text=="nothing")left={{"kind","literal"},{"value",token.text=="nothing"?json(nullptr):json(token.text=="true")}};else left={{"kind","var"},{"name",token.text}};}
  else throw Error{line,"Unexpected “"+token.text+"” in this expression."};
  while(i<end){
   if(at("(")&&90>=min){i++;json args=json::array();if(!take(")")){do{args.push_back(parse());}while(take(","));need(")");}left={{"kind","call"},{"callee",std::move(left)},{"args",std::move(args)}};continue;}
   if(at("[")&&90>=min){i++;json index=parse();need("]");left={{"kind","index"},{"object",std::move(left)},{"index",std::move(index)}};continue;}
   std::string op=t[i].text;size_t count=1;
   static const std::vector<std::pair<std::string,std::string>> phrases={{"is not","!="},{"is at least",">="},{"is at most","<="},{"is greater than",">"},{"is less than","<"},{"is","=="}};for(auto&pair:phrases)if(at(pair.first)){op=pair.second;count=1+std::count(pair.first.begin(),pair.first.end(),' ');break;}
   static const std::unordered_map<std::string,int>prec={{"or",10},{"and",20},{"==",30},{"!=",30},{"<",30},{">",30},{"<=",30},{">=",30},{"in",30},{"+",40},{"-",40},{"*",50},{"/",50},{"//",50},{"%",50},{"**",70}};
   auto p=prec.find(op);if(p==prec.end()||p->second<min)break;i+=count;json right=parse(p->second+(op=="**"?0:1));left={{"kind","binary"},{"op",op},{"left",std::move(left)},{"right",std::move(right)}};
  }
  return left;
 }
 json full(){json e=parse();if(i!=end)throw Error{line,"Unexpected “"+t[i].text+"”. Check the expression."};return e;}
};
struct SourceParser {
 struct Row{int line;size_t indent;Tokens tokens;};std::vector<Row>rows;size_t pos=0;
 explicit SourceParser(std::string source){
  if(source.compare(0,3,"\xef\xbb\xbf")==0)source.erase(0,3);
  std::string normalized;normalized.reserve(source.size());for(size_t i=0;i<source.size();i++){if(source[i]=='\r'){normalized+='\n';if(i+1<source.size()&&source[i+1]=='\n')i++;}else normalized+=source[i];}
  std::istringstream stream(normalized);std::string raw;int line=0;
  while(std::getline(stream,raw)){line++;size_t indent=0;while(indent<raw.size()&&raw[indent]==' ')indent++;if(indent<raw.size()&&raw[indent]=='\t')throw Error{line,"Use spaces instead of tabs for indentation."};Tokens t=tokenize(raw,line);if(!t.empty())rows.push_back({line,indent,std::move(t)});}
 }
 json block(size_t indent,int loop=0,bool fn=false){
  json nodes=json::array();while(pos<rows.size()&&rows[pos].indent>=indent){
   const Row&r=rows[pos++];if(r.indent!=indent)throw Error{r.line,"This line is indented too far. Align it with the other lines in this block."};Tokens t=r.tokens;int line=r.line;bool colon=t.back().text==":";if(colon)t.pop_back();
   auto is=[&](const std::string&s){return matches(t,0,s);};
   auto expr=[&](size_t start=0,size_t end=SIZE_MAX){return ExpressionParser(t,line,start,end==SIZE_MAX?t.size():end).full();};
   auto name=[&](size_t i){if(i>=t.size()||t[i].type!="id"||t[i].text=="true"||t[i].text=="false"||t[i].text=="nothing"||t[i].text=="and"||t[i].text=="or"||t[i].text=="not")throw Error{line,"Use a name made of letters, numbers, and underscores, starting with a letter."};return t[i].text;};
   auto find=[&](const std::string&s,size_t start=0){int depth=0;for(size_t i=start;i<t.size();i++){if(t[i].type=="op"&&(t[i].text=="("||t[i].text=="["))depth++;if(t[i].type=="op"&&(t[i].text==")"||t[i].text=="]"))depth--;if(depth==0&&matches(t,i,s))return (int)i;}return -1;};
   auto body=[&](int childLoop,bool childFn){if(!colon)throw Error{line,"Add a colon (:) at the end of this line to start its block."};if(pos>=rows.size()||rows[pos].indent<=indent)throw Error{line,"Add an indented instruction below this line."};return block(rows[pos].indent,childLoop,childFn);};
   json n;
   if(is("if")){json test=expr(1),yes=body(loop,fn);n={{"kind","if"},{"test",std::move(test)},{"yes",std::move(yes)},{"no",json::array()}};if(pos<rows.size()&&rows[pos].indent==indent&&matches(rows[pos].tokens,0,"otherwise")){const Row&o=rows[pos++];if(o.tokens.size()!=2||o.tokens[1].text!=":")throw Error{o.line,"Write “otherwise:” on its own line."};if(pos>=rows.size()||rows[pos].indent<=indent)throw Error{o.line,"Add an indented instruction below otherwise."};n["no"]=block(rows[pos].indent,loop,fn);}}
   else if(is("repeat")){if(t.empty()||t.back().text!="times")throw Error{line,"Write “repeat 4 times:” followed by an indented block."};json count=expr(1,t.size()-1);n={{"kind","repeat"},{"count",std::move(count)},{"body",body(loop+1,fn)}};}
   else if(is("while")){json test=expr(1);n={{"kind","while"},{"test",std::move(test)},{"body",body(loop+1,fn)}};}
   else if(is("for each")){std::string nm=name(2);if(!matches(t,3,"in"))throw Error{line,"Write “for each item in items:”."};json items=expr(4);n={{"kind","for"},{"name",nm},{"items",std::move(items)},{"body",body(loop+1,fn)}};}
   else if(is("function")){std::string nm=name(1);if(t.size()<4||t[2].text!="("||t.back().text!=")")throw Error{line,"Write “function name(parameter):”."};json params=json::array();std::unordered_set<std::string>seen;size_t i=3;while(i<t.size()-1){std::string p=name(i++);if(!seen.insert(p).second)throw Error{line,"Each parameter needs a different name."};params.push_back(p);if(i<t.size()-1&&t[i++].text!=",")throw Error{line,"Separate parameters with commas."};}n={{"kind","function"},{"name",nm},{"params",std::move(params)},{"body",body(0,true)}};}
   else if(is("otherwise"))throw Error{line,"“otherwise:” must come directly after an if block at the same indentation."};
   else {
    if(colon)throw Error{line,"This instruction does not start a block. Remove the colon."};
    if(is("set pen color to")||is("set pen size to"))n={{"kind","draw"},{"op",t[2].text=="color"?"color":"size"},{"value",expr(4)}};
    else if(is("set")){std::string nm=name(1);if(!matches(t,2,"to"))throw Error{line,"Write “set name to value”."};n={{"kind","set"},{"name",nm},{"value",expr(3)}};}
    else if(is("say"))n={{"kind","say"},{"value",expr(1)}};
    else if(is("wait")){std::string unit=t.back().text;if(t.back().type!="id"||(unit!="second"&&unit!="seconds"&&unit!="millisecond"&&unit!="milliseconds"))throw Error{line,"Write “wait 1 second”, “wait 0.5 seconds”, or “wait 100 milliseconds”."};n={{"kind","wait"},{"value",expr(1,t.size()-1)},{"scale",unit[0]=='s'?1000:1}};}
    else if(is("clear output")){if(t.size()!=2)throw Error{line,"Write just “clear output”."};n={{"kind","clearOutput"}};}
    else if(is("stop")){if(t.size()!=1)throw Error{line,"Write just “stop” to finish your program."};n={{"kind","stop"}};}
    else if(is("ask")){int p=find("and store",1);if(p<0)throw Error{line,"Write “ask \"Question?\" and store in answer”."};size_t idx=p+2;bool number=matches(t,idx,"as number");if(number)idx+=2;if(!matches(t,idx,"in")||idx+2!=t.size())throw Error{line,"End this instruction with “in name”."};n={{"kind","ask"},{"prompt",expr(1,p)},{"name",name(idx+1)},{"number",number}};}
    else if(is("add")||is("subtract")){std::string op=t[0].text;int p=find(op=="add"?"to":"from",1);if(p<0||(size_t)p+2!=t.size())throw Error{line,"Write “"+op+" value "+(op=="add"?"to":"from")+" name”."};n={{"kind","change"},{"op",op},{"name",name(p+1)},{"value",expr(1,p)}};}
    else if(is("append")){int p=find("to",1);if(p<0||(size_t)p+2!=t.size())throw Error{line,"Write “append value to list_name”."};n={{"kind","append"},{"name",name(p+1)},{"value",expr(1,p)}};}
    else if(is("return")){if(!fn)throw Error{line,"“return” belongs inside a function."};n={{"kind","return"},{"value",t.size()>1?expr(1):json{{"kind","literal"},{"value",nullptr}}}};}
    else if(is("break")||is("continue")){if(!loop||t.size()!=1)throw Error{line,"Use this instruction by itself inside a loop."};n={{"kind",t[0].text}};}
    else if(is("move forward")||is("move backward")||is("move backwards")||is("turn right")||is("turn left")){std::string op=t[0].text=="turn"?t[1].text:t[1].text=="forward"?"move":"back";n={{"kind","draw"},{"op",op},{"value",expr(2)}};}
    else if(is("pen up")||is("pen down")){if(t.size()!=2)throw Error{line,"Write just “pen up” or “pen down”."};n={{"kind","draw"},{"op",t[1].text}};}
    else if(is("clear drawing")){if(t.size()!=2)throw Error{line,"Write just “clear drawing”."};n={{"kind","draw"},{"op","clear"}};}
    else if(is("go to")){int p=find(",",2);if(p<0)throw Error{line,"Write “go to x, y”."};n={{"kind","draw"},{"op","goto"},{"value",expr(2,p)},{"y",expr(p+1)}};}
    else {if(!t.empty()&&t[0].type=="id"&&(t.size()<2||t[1].text!="("))throw Error{line,"Unknown instruction “"+t[0].text+"”. Try say, set, wait, if, repeat, or a function call."};json value=expr();if(value["kind"]!="call")throw Error{line,"Start with an instruction such as “say”, “set”, or a function call."};n={{"kind","expression"},{"value",std::move(value)}};}
   }
   n["line"]=line;nodes.push_back(std::move(n));
  }
  return nodes;
 }
 json parse(){if(!rows.empty()&&rows[0].indent)throw Error{rows[0].line,"Start the first instruction at the left edge."};return block(0);}
};
