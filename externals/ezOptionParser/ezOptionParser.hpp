/*
This file is part of ezOptionParser. See MIT-LICENSE.

Copyright (C) 2011,2012,2014 Remik Ziemlinski <first d0t surname att gmail>

CHANGELOG

v0.0.0 20110505 rsz Created.
v0.1.0 20111006 rsz Added validator.
v0.1.1 20111012 rsz Fixed validation of ulonglong.
v0.1.2 20111126 rsz Allow flag names start with alphanumeric (previously, flag had to start with alpha).
v0.1.3 20120108 rsz Created work-around for unique id generation with IDGenerator that avoids retarded c++ translation unit linker errors with single-header static variables. Forced inline on all methods to please retard compiler and avoid multiple def errors.
v0.1.4 20120629 Enforced MIT license on all files.
v0.2.0 20121120 Added parseIndex to OptionGroup.
v0.2.1 20130506 Allow disabling doublespace of OPTIONS usage descriptions.
v0.2.2 20140504 Jose Santiago added compiler warning fixes.
                Bruce Shankle added a crash fix in description printing.
*/
#ifndef EZ_OPTION_PARSER_H
#define EZ_OPTION_PARSER_H

#include <stdlib.h>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <limits>
#include <sstream>
#include <cstring>

namespace ez {
#define DEBUGLINE() printf("%s:%d\n", __FILE__, __LINE__);

/* ################################################################### */
template<typename T>
static T fromString(const std::string* s) {
  std::istringstream stream (s->c_str());
  T t;
  stream >> t;
  return t;
};
template<typename T>
static T fromString(const char* s) {
  std::istringstream stream (s);
  T t;
  stream >> t;
  return t;
};
/* ################################################################### */
static inline bool isdigit(const std::string & s, int i=0) {
  int n = s.length();
  for(; i < n; ++i)
    switch(s[i]) {
      case '0': case '1': case '2':
      case '3': case '4': case '5':
      case '6': case '7': case '8': case '9': break;
      default: return false;
    }
    
  return true;
};
/* ################################################################### */
static bool isdigit(const std::string * s, int i=0) {
  int n = s->length();
  for(; i < n; ++i)
    switch(s->at(i)) {
      case '0': case '1': case '2':
      case '3': case '4': case '5':
      case '6': case '7': case '8': case '9': break;
      default: return false;
    }
    
  return true;
};
/* ################################################################### */
/* 
Compare strings for opts, so short opt flags come before long format flags.
For example, -d < --dimension < --dmn, and also lower come before upper. The default STL std::string compare doesn't do that.
*/
static bool CmpOptStringPtr(std::string * s1, std::string * s2) { 
  int c1,c2;
  const char *s=s1->c_str();
  for(c1=0; c1 < (long int)s1->size(); ++c1)
    if (isalnum(s[c1])) // locale sensitive.
      break;
      
  s=s2->c_str();
  for(c2=0; c2 < (long int)s2->size(); ++c2)
    if (isalnum(s[c2]))
      break;

  // Test which has more symbols before its name.
  if (c1 > c2)
    return false;
  else if (c1 < c2)
    return true;
    
  // Both have same number of symbols, so compare first letter.
  char char1 = s1->at(c1);
  char char2 = s2->at(c2);
  char lo1 = tolower(char1);
  char lo2 = tolower(char2);

  if (lo1 != lo2)
    return lo1 < lo2;

  // Their case doesn't match, so find which is lower.
  char up1 = isupper(char1);
  char up2 = isupper(char2);
    
  if (up1 && !up2)
    return false;
  else if (!up1 && up2)
    return true;
    
  return (s1->compare(*s2)<0); 
};
/* ################################################################### */
/* 
Makes a vector of strings from one string,
splitting at (and excluding) delimiter "token".
*/
static void SplitDelim( const std::string& s, const char token, std::vector<std::string*> * result) {
  std::string::const_iterator i = s.begin();
  std::string::const_iterator j = s.begin();
  const std::string::const_iterator e = s.end();

  while(i!=e) {
    while(i!=e && *i++!=token);
    std::string *newstr = new std::string(j, i);
    if (newstr->at(newstr->size()-1) == token) newstr->erase(newstr->size()-1);
    result->push_back(newstr);
    j = i;
  }
};
/* ################################################################### */
// Variant that uses deep copies and references instead of pointers (less efficient).
static void SplitDelim( const std::string& s, const char token, std::vector<std::string> & result) {
  std::string::const_iterator i = s.begin();
  std::string::const_iterator j = s.begin();
  const std::string::const_iterator e = s.end();

  while(i!=e) {
    while(i!=e && *i++!=token);
    std::string newstr(j, i);
    if (newstr.at(newstr.size()-1) == token) newstr.erase(newstr.size()-1);
    result.push_back(newstr);
    j = i;
  }
};
/* ################################################################### */
// Variant that uses list instead of vector for efficient insertion, etc.
static void SplitDelim( const std::string& s, const char token, std::list<std::string*> & result) {
  std::string::const_iterator i = s.begin();
  std::string::const_iterator j = s.begin();
  const std::string::const_iterator e = s.end();

  while(i!=e) {
    while(i!=e && *i++!=token);
    std::string *newstr = new std::string(j, i);
    if (newstr->at(newstr->size()-1) == token) newstr->erase(newstr->size()-1);
    result.push_back(newstr);
    j = i;
  }
};
/* ################################################################### */
static void ToU1(std::string ** strings, unsigned char * out, int n) {
  for(int i=0; i < n; ++i) {
    out[i] = (unsigned char)atoi(strings[i]->c_str());
  }
};
/* ################################################################### */
static void ToS1(std::string ** strings, char * out, int n) {
  for(int i=0; i < n; ++i) {
    out[i] = (char)atoi(strings[i]->c_str());
  }
};
/* ################################################################### */
static void ToU2(std::string ** strings, unsigned short * out, int n) {
  for(int i=0; i < n; ++i) {
    out[i] = (unsigned short)atoi(strings[i]->c_str());
  }
};
/* ################################################################### */
static void ToS2(std::string ** strings, short * out, int n) {
  for(int i=0; i < n; ++i) {
    out[i] = (short)atoi(strings[i]->c_str());
  }
};
/* ################################################################### */
static void ToS4(std::string ** strings, int * out, int n) {
  for(int i=0; i < n; ++i) {
    out[i] = atoi(strings[i]->c_str());
  }
};
/* ################################################################### */
static void ToU4(std::string ** strings, unsigned int * out, int n) {
  for(int i=0; i < n; ++i) {
    out[i] = (unsigned int)strtoul(strings[i]->c_str(), NULL, 0);
  }
};
/* ################################################################### */
static void ToS8(std::string ** strings, long long * out, int n) {
  for(int i=0; i < n; ++i) {
    std::stringstream ss(strings[i]->c_str());
    ss >> out[i];
  }
};
/* ################################################################### */
static void ToU8(std::string ** strings, unsigned long long * out, int n) {
  for(int i=0; i < n; ++i) {
    std::stringstream ss(strings[i]->c_str());
    ss >> out[i];
  }
};
/* ################################################################### */
static void ToF(std::string ** strings, float * out, int n) {
  for(int i=0; i < n; ++i) {
    out[i] = (float)atof(strings[i]->c_str());
  }
};
/* ################################################################### */
static void ToD(std::string ** strings, double * out, int n) {
  for(int i=0; i < n; ++i) {
    out[i] = (double)atof(strings[i]->c_str());
  }
};
/* ################################################################### */
static void StringsToInts(std::vector<std::string> & strings, std::vector<int> & out) {
  for(int i=0; i < (long int)strings.size(); ++i) {
    out.push_back(atoi(strings[i].c_str()));
  }
};
/* ################################################################### */
static void StringsToInts(std::vector<std::string*> * strings, std::vector<int> * out) {
  for(int i=0; i < (long int)strings->size(); ++i) {
    out->push_back(atoi(strings->at(i)->c_str()));
  }
};
/* ################################################################### */
static void StringsToLongs(std::vector<std::string> & strings, std::vector<long> & out) {
  for(int i=0; i < (long int)strings.size(); ++i) {
    out.push_back(atol(strings[i].c_str()));
  }
};
/* ################################################################### */
static void StringsToLongs(std::vector<std::string*> * strings, std::vector<long> * out) {
  for(int i=0; i < (long int)strings->size(); ++i) {
    out->push_back(atol(strings->at(i)->c_str()));
  }
};
/* ################################################################### */
static void StringsToULongs(std::vector<std::string> & strings, std::vector<unsigned long> & out) {
  for(int i=0; i < (long int)strings.size(); ++i) {
    out.push_back(strtoul(strings[i].c_str(),0,0));
  }
};
/* ################################################################### */
static void StringsToULongs(std::vector<std::string*> * strings, std::vector<unsigned long> * out) {
  for(int i=0; i < (long int)strings->size(); ++i) {
    out->push_back(strtoul(strings->at(i)->c_str(),0,0));
  }
};
/* ################################################################### */
static void StringsToFloats(std::vector<std::string> & strings, std::vector<float> & out) {
  for(int i=0; i < (long int)strings.size(); ++i) {
    out.push_back(atof(strings[i].c_str()));
  }
};
/* ################################################################### */
static void StringsToFloats(std::vector<std::string*> * strings, std::vector<float> * out) {
  for(int i=0; i < (long int)strings->size(); ++i) {
    out->push_back(atof(strings->at(i)->c_str()));
  }
};
/* ################################################################### */
static void StringsToDoubles(std::vector<std::string> & strings, std::vector<double> & out) {
  for(int i=0; i < (long int)strings.size(); ++i) {
    out.push_back(atof(strings[i].c_str()));
  }
};
/* ################################################################### */
static void StringsToDoubles(std::vector<std::string*> * strings, std::vector<double> * out) {
  for(int i=0; i < (long int)strings->size(); ++i) {
    out->push_back(atof(strings->at(i)->c_str()));
  }
};
/* ################################################################### */
static void StringsToStrings(std::vector<std::string*> * strings, std::vector<std::string> * out) {
  for(int i=0; i < (long int)strings->size(); ++i) {
    out->push_back( *strings->at(i) );
  }
};
/* ################################################################### */
static void ToLowerASCII(std::string & s) {
  int n = s.size();
  int i=0;
  char c;
  for(; i < n; ++i) {
    c = s[i];
    if(c<='Z' && c>='A')
      s[i] = c+32;
  }
}
/* ################################################################### */
static char** CommandLineToArgvA(char* CmdLine, int* _argc) {
  char** argv;
  char*  _argv;
  unsigned long   len;
  unsigned long   argc;
  char   a;
  unsigned long   i, j;

  bool  in_QM;
  bool  in_TEXT;
  bool  in_SPACE;

  len = strlen(CmdLine);
  i = ((len+2)/2)*sizeof(void*) + sizeof(void*);

  argv = (char**)malloc(i + (len+2)*sizeof(char));

  _argv = (char*)(((unsigned char*)argv)+i);

  argc = 0;
  argv[argc] = _argv;
  in_QM = false;
  in_TEXT = false;
  in_SPACE = true;
  i = 0;
  j = 0;

  while( (a = CmdLine[i]) ) {
    if(in_QM) {
      if( (a == '\"') ||
          (a == '\'')) // rsz. Added single quote.
      {
        in_QM = false;
      } else {
        _argv[j] = a;
        j++;
      }
    } else {
      switch(a) {
      case '\"': 
      case '\'': // rsz. Added single quote.
        in_QM = true;
        in_TEXT = true;
        if(in_SPACE) {
          argv[argc] = _argv+j;
          argc++;
        }
        in_SPACE = false;
        break;
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        if(in_TEXT) {
          _argv[j] = '\0';
          j++;
        }
        in_TEXT = false;
        in_SPACE = true;
        break;
      default:
        in_TEXT = true;
        if(in_SPACE) {
          argv[argc] = _argv+j;
          argc++;
        }
        _argv[j] = a;
        j++;
        in_SPACE = false;
        break;
      }
    }
    i++;
  }
  _argv[j] = '\0';
  argv[argc] = NULL;

  (*_argc) = argc;
  return argv;
};
/* ################################################################### */
// Create unique ids with static and still allow single header that avoids multiple definitions linker error.
class ezOptionParserIDGenerator {
public:
  static ezOptionParserIDGenerator& instance () { static ezOptionParserIDGenerator Generator; return Generator; }
  short next () { return ++_id; }
private:
  ezOptionParserIDGenerator() : _id(-1) {}
  short _id;
};
/* ################################################################### */
/* Validate a value by checking:
- if as string, see if converted value is within datatype's limits,
- and see if falls within a desired range,
- or see if within set of given list of values.

If comparing with a range, the values list must contain one or two values. One value is required when comparing with <, <=, >, >=. Use two values when requiring a test such as <x<, <=x<, <x<=, <=x<=.
A regcomp/regexec based class could be created in the future if a need arises.
*/
class ezOptionValidator {
public:
  inline ezOptionValidator(const char* _type, const char* _op=0, const char* list=0, bool _insensitive=false);
  inline ezOptionValidator(char _type);
  inline ezOptionValidator(char _type, char _op, const char* list, int _size);
  inline ezOptionValidator(char _type, char _op, const unsigned char* list, int _size);
  inline ezOptionValidator(char _type, char _op, const short* list, int _size);
  inline ezOptionValidator(char _type, char _op, const unsigned short* list, int _size);
  inline ezOptionValidator(char _type, char _op, const int* list, int _size);
  inline ezOptionValidator(char _type, char _op, const unsigned int* list, int _size);
  inline ezOptionValidator(char _type, char _op, const long long* list, int _size);
  inline ezOptionValidator(char _type, char _op, const unsigned long long* list, int _size=0);
  inline ezOptionValidator(char _type, char _op, const float* list, int _size);
  inline ezOptionValidator(char _type, char _op, const double* list, int _size);
  inline ezOptionValidator(char _type, char _op, const char** list, int _size, bool _insensitive);
  inline ~ezOptionValidator();

  inline bool isValid(const std::string * value);
  inline void print();
  inline void reset();
  
  /* If value must be in custom range, use these comparison modes. */
  enum OP { NOOP=0, 
    LT, /* value < list[0] */
    LE, /* value <= list[0] */
    GT, /* value > list[0] */
    GE, /* value >= list[0] */
    GTLT, /* list[0] < value < list[1] */
    GELT, /* list[0] <= value < list[1] */
    GELE, /* list[0] <= value <= list[1] */
    GTLE, /* list[0] < value <= list[1] */
    IN /* if value is in list */
  };
  
  enum TYPE { NOTYPE=0, S1, U1, S2, U2, S4, U4, S8, U8, F, D, T };
  enum TYPE2 { NOTYPE2=0, INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT, DOUBLE, TEXT };
    
  union {
    unsigned char *u1;
    char *s1;
    unsigned short *u2;
    short *s2;
    unsigned int *u4;
    int *s4;
    unsigned long long *u8;
    long long *s8;
    float *f;
    double *d;
    std::string** t;
  };  
  
  char op;
  bool quiet;
  short id;
  char type;
  int size;
  bool insensitive;
};
/* ------------------------------------------------------------------- */
ezOptionValidator::~ezOptionValidator() {
  reset();
};
/* ------------------------------------------------------------------- */
void ezOptionValidator::reset() {
  #define CLEAR(TYPE,P) case TYPE: if (P) delete [] P; P = 0; break;
  switch(type) {
    CLEAR(S1,s1);
    CLEAR(U1,u1);
    CLEAR(S2,s2);
    CLEAR(U2,u2);
    CLEAR(S4,s4);
    CLEAR(U4,u4);
    CLEAR(S8,s8);
    CLEAR(U8,u8);
    CLEAR(F,f);
    CLEAR(D,d);
    case T:
      for(int i=0; i < size; ++i)
        delete t[i];
        
      delete [] t;
      t = 0;
      break;
    default: break;
  }
   
  size = 0;
  op = NOOP;
  type = NOTYPE;
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type) : s1(0), op(0), quiet(0), type(_type), size(0), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const char* list, int _size) : s1(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  s1 = new char[size];
  memcpy(s1, list, size);
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const unsigned char* list, int _size) : u1(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  u1 = new unsigned char[size];
  memcpy(u1, list, size);
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const short* list, int _size) : s2(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  s2 = new short[size];
  memcpy(s2, list, size*sizeof(short));
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const unsigned short* list, int _size) : u2(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  u2 = new unsigned short[size];
  memcpy(u2, list, size*sizeof(unsigned short));
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const int* list, int _size) : s4(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  s4 = new int[size];
  memcpy(s4, list, size*sizeof(int));
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const unsigned int* list, int _size) : u4(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  u4 = new unsigned int[size];
  memcpy(u4, list, size*sizeof(unsigned int));
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const long long* list, int _size) : s8(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  s8 = new long long[size];
  memcpy(s8, list, size*sizeof(long long));
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const unsigned long long* list, int _size) : u8(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  u8 = new unsigned long long[size];
  memcpy(u8, list, size*sizeof(unsigned long long));
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const float* list, int _size) : f(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  f = new float[size];
  memcpy(f, list, size*sizeof(float));
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const double* list, int _size) : d(0), op(_op), quiet(0), type(_type), size(_size), insensitive(0) {
  id = ezOptionParserIDGenerator::instance().next();
  d = new double[size];
  memcpy(d, list, size*sizeof(double));
};
/* ------------------------------------------------------------------- */
ezOptionValidator::ezOptionValidator(char _type, char _op, const char** list, int _size, bool _insensitive) : t(0), op(_op), quiet(0), type(_type), size(_size), insensitive(_insensitive) {
  id = ezOptionParserIDGenerator::instance().next();
  t = new std::string*[size];
  int i=0;

  for(; i < size; ++i) {
    t[i] = new std::string(list[i]);
  }
};
/* ------------------------------------------------------------------- */
/* Less efficient but convenient ctor that parses strings to setup validator. 
_type: s1, u1, s2, u2, ..., f, d, t
_op: lt, gt, ..., in
_list: comma-delimited string
*/
ezOptionValidator::ezOptionValidator(const char* _type, const char* _op, const char* _list, bool _insensitive) : t(0), quiet(0), type(0), size(0), insensitive(_insensitive) {
  id = ezOptionParserIDGenerator::instance().next();
  
  switch(_type[0]) {
    case 'u':
      switch(_type[1]) {
        case '1': type = U1; break;
        case '2': type = U2; break;
        case '4': type = U4; break;
        case '8': type = U8; break;
        default: break;
      } 
      break;
    case 's':
      switch(_type[1]) {
        case '1': type = S1; 
          break;
        case '2': type = S2; break;
        case '4': type = S4; break;
        case '8': type = S8; break;
        default: break;
      }
      break;
    case 'f': type = F; break;
    case 'd': type = D; break;
    case 't': type = T; break;
    default: 
      if (!quiet)
        std::cerr << "ERROR: Unknown validator datatype \"" << _type << "\".\n";
      break;
  }
  
  int nop = 0;
  if (_op != 0)
    nop = strlen(_op);
  
  switch(nop) {
    case 0: op = NOOP; break;
    case 2:
      switch(_op[0]) {
        case 'g':
          switch(_op[1]) {
            case 'e': op = GE; break;
            default: op = GT; break;
          } 
          break;
        case 'i': op = IN;
          break;
        default:
          switch(_op[1]) {
            case 'e': op = LE; break;
            default: op = LT; break;
          } 
          break;
      }
      break;
    case 4:
      switch(_op[1]) {
        case 'e':
          switch(_op[3]) {
            case 'e': op = GELE; break;
            default: op = GELT; break;
          }
          break;
        default:
          switch(_op[3]) {
            case 'e': op = GTLE; break;
            default: op = GTLT; break;
          }
          break;
      }
      break;
    default: 
      if (!quiet)
        std::cerr << "ERROR: Unknown validator operation \"" << _op << "\".\n";
      break;
  }

  if (_list == 0) return;
  // Create list of strings and then cast to native datatypes.
  std::string unsplit(_list);
  std::list<std::string*> split;
  std::list<std::string*>::iterator it;
  SplitDelim(unsplit, ',', split);
  size = split.size();
  std::string **strings = new std::string*[size];

  int i = 0;
  for(it = split.begin(); it != split.end(); ++it) 
    strings[i++] = *it;
    
  if (insensitive) 
    for(i=0; i < size; ++i) 
      ToLowerASCII(*strings[i]);
  
  #define FreeStrings() { \
    for(i=0; i < size; ++i)\
      delete strings[i];\
    delete [] strings;\
  }
    
  #define ToArray(T,P,Y) case T: P = new Y[size]; To##T(strings, P, size); FreeStrings(); break;
  switch(type) {
    ToArray(S1,s1,char);
    ToArray(U1,u1,unsigned char);
    ToArray(S2,s2,short);
    ToArray(U2,u2,unsigned short);
    ToArray(S4,s4,int);
    ToArray(U4,u4,unsigned int);
    ToArray(S8,s8,long long);
    ToArray(U8,u8,unsigned long long);
    ToArray(F,f,float);
    ToArray(D,d,double);
    case T: t = strings; break; /* Don't erase strings array. */
    default: break;
  }   
};
/* ------------------------------------------------------------------- */
void ezOptionValidator::print() {
  printf("id=%d, op=%d, type=%d, size=%d, insensitive=%d\n", id, op, type, size, insensitive);
};
/* ------------------------------------------------------------------- */
bool ezOptionValidator::isValid(const std::string * valueAsString) {
  if (valueAsString == 0) return false;

#define CHECKRANGE(E,T) {\
  std::stringstream ss(valueAsString->c_str()); \
  long long E##value; \
  ss >> E##value; \
  long long E##min = static_cast<long long>(std::numeric_limits<T>::min()); \
  if (E##value < E##min) { \
    if (!quiet) \
      std::cerr << "ERROR: Invalid value " << E##value << " is less than datatype min " << E##min << ".\n"; \
    return false; \
  } \
  \
  long long E##max = static_cast<long long>(std::numeric_limits<T>::max()); \
  if (E##value > E##max) { \
    if (!quiet) \
      std::cerr << "ERROR: Invalid value " << E##value << " is greater than datatype max " << E##max << ".\n"; \
    return false; \
  } \
}
  // Check if within datatype limits.
  if (type != T) {
    switch(type) {
      case S1: CHECKRANGE(S1,char); break;
      case U1: CHECKRANGE(U1,unsigned char); break;
      case S2: CHECKRANGE(S2,short); break;
      case U2: CHECKRANGE(U2,unsigned short); break;
      case S4: CHECKRANGE(S4,int); break;
      case U4: CHECKRANGE(U4,unsigned int); break;
      case S8: {
        if ( (valueAsString->at(0) == '-') &&
              isdigit(valueAsString,1) &&
             (valueAsString->size() > 19) &&
             (valueAsString->compare(1, 19, "9223372036854775808") > 0) ) {
          if (!quiet)
            std::cerr << "ERROR: Invalid value " << *valueAsString << " is less than datatype min -9223372036854775808.\n";
          return false;
        }

        if (isdigit(valueAsString) && 
            (valueAsString->size() > 18) && 
            valueAsString->compare("9223372036854775807") > 0) {
          if (!quiet)
            std::cerr << "ERROR: Invalid value " << *valueAsString << " is greater than datatype max 9223372036854775807.\n";
          return false;
        }
        } break;
      case U8: {
        if (valueAsString->compare("0") < 0) {
          if (!quiet)
            std::cerr << "ERROR: Invalid value " << *valueAsString << " is less than datatype min 0.\n";
          return false;
        }
        
        if (isdigit(valueAsString) &&
           (valueAsString->size() > 19) && 
            valueAsString->compare("18446744073709551615") > 0) {
          if (!quiet)
            std::cerr << "ERROR: Invalid value " << *valueAsString << " is greater than datatype max 18446744073709551615.\n";
          return false;
        }
        } break;
      case F: {
        double dmax = static_cast<double>(std::numeric_limits<float>::max());
        double dvalue = atof(valueAsString->c_str());
        double dmin = -dmax;
        if (dvalue < dmin) {
          if (!quiet) {
            fprintf(stderr, "ERROR: Invalid value %g is less than datatype min %g.\n", dvalue, dmin);
          }
          return false;
        }

        if (dvalue > dmax) {
          if (!quiet)
            std::cerr << "ERROR: Invalid value " << dvalue << " is greater than datatype max " << dmax << ".\n";
          return false;
        }
        } break;
      case D: {
        long double ldmax = static_cast<long double>(std::numeric_limits<double>::max()); 
        std::stringstream ss(valueAsString->c_str());
        long double ldvalue;
        ss >> ldvalue;
        long double ldmin = -ldmax;
        
        if (ldvalue < ldmin) {
          if (!quiet)
            std::cerr << "ERROR: Invalid value " << ldvalue << " is less than datatype min " << ldmin << ".\n";
          return false;
        }

        if (ldvalue > ldmax) {
          if (!quiet)
            std::cerr << "ERROR: Invalid value " << ldvalue << " is greater than datatype max " << ldmax << ".\n";
          return false;
        }
        } break;
      case NOTYPE: default: break;
    }
  } else {
    if (op == IN) { 
		  int i=0;
      if (insensitive) {
        std::string valueAsStringLower(*valueAsString);
        ToLowerASCII(valueAsStringLower);
        for(; i < size; ++i) {
          if (valueAsStringLower.compare(t[i]->c_str()) == 0)
            return true;
        }
      } else {
        for(; i < size; ++i) {
          if (valueAsString->compare(t[i]->c_str()) == 0)
            return true;
        }
      }
      return false;
    } 
  }
  
  // Only check datatype limits, and return;
  if (op == NOOP) return true;
  
#define VALIDATE(T, U, LIST) { \
  /* Value string converted to true native type. */ \
  std::stringstream ss(valueAsString->c_str());\
  U v;\
  ss >> v;\
  /* Check if within list. */ \
  if (op == IN) { \
    T * last = LIST + size;\
    return (last != std::find(LIST, last, v)); \
  } \
  \
  /* Check if within user's custom range. */ \
  T v0, v1; \
  if (size > 0) { \
    v0 = LIST[0]; \
  } \
  \
  if (size > 1) { \
    v1 = LIST[1]; \
  } \
  \
  switch (op) {\
    case LT:\
      if (size > 0) {\
        return v < v0;\
      } else {\
        std::cerr << "ERROR: No value given to validate if " << v << " < X.\n";\
        return false;\
      }\
      break;\
    case LE:\
      if (size > 0) {\
        return v <= v0;\
      } else {\
        std::cerr << "ERROR: No value given to validate if " << v << " <= X.\n";\
        return false;\
      }\
      break;\
    case GT:\
      if (size > 0) {\
        return v > v0;\
      } else {\
        std::cerr << "ERROR: No value given to validate if " << v << " > X.\n";\
        return false;\
      }\
      break;\
    case GE:\
      if (size > 0) {\
        return v >= v0;\
      } else {\
        std::cerr << "ERROR: No value given to validate if " << v << " >= X.\n";\
        return false;\
      }\
      break;\
    case GTLT:\
      if (size > 1) {\
        return (v0 < v) && (v < v1);\
      } else {\
        std::cerr << "ERROR: Missing values to validate if X1 < " << v << " < X2.\n";\
        return false;\
      }\
      break;\
    case GELT:\
      if (size > 1) {\
        return (v0 <= v) && (v < v1);\
      } else {\
        std::cerr << "ERROR: Missing values to validate if X1 <= " << v << " < X2.\n";\
        return false;\
      }\
      break;\
    case GELE:\
      if (size > 1) {\
        return (v0 <= v) && (v <= v1);\
      } else {\
        std::cerr << "ERROR: Missing values to validate if X1 <= " << v << " <= X2.\n";\
        return false;\
      }\
      break;\
    case GTLE:\
      if (size > 1) {\
        return (v0 < v) && (v <= v1);\
      } else {\
        std::cerr << "ERROR: Missing values to validate if X1 < " << v << " <= X2.\n";\
        return false;\
      }\
      break;\
      case NOOP: case IN: default: break;\
  } \
  }
  
  switch(type) {
    case U1: VALIDATE(unsigned char, int, u1); break;
    case S1: VALIDATE(char, int, s1); break;
    case U2: VALIDATE(unsigned short, int, u2); break;
    case S2: VALIDATE(short, int, s2); break;
    case U4: VALIDATE(unsigned int, unsigned int, u4); break;
    case S4: VALIDATE(int, int, s4); break;
    case U8: VALIDATE(unsigned long long, unsigned long long, u8); break;
    case S8: VALIDATE(long long, long long, s8); break;
    case F: VALIDATE(float, float, f); break;
    case D: VALIDATE(double, double, d); break;
    default: break;
  }

  return true;
};
/* ################################################################### */
class OptionGroup {
public:
  OptionGroup() : delim(0), expectArgs(0), isRequired(false), isSet(false) { }

  ~OptionGroup() {
    int i;
    for(i=0; i < (long int)flags.size(); ++i)
      delete flags[i];
      
    flags.clear();
    parseIndex.clear();
    clearArgs();
  };

  inline void clearArgs();
  inline void getInt(int&);
  inline void getLong(long&);
  inline void getLongLong(long long&);
  inline void getULong(unsigned long&);
  inline void getULongLong(unsigned long long&);
  inline void getFloat(float&);
  inline void getDouble(double&);
  inline void getString(std::string&);
  inline void getInts(std::vector<int>&);
  inline void getLongs(std::vector<long>&);
  inline void getULongs(std::vector<unsigned long>&);
  inline void getFloats(std::vector<float>&);
  inline void getDoubles(std::vector<double>&);
  inline void getStrings(std::vector<std::string>&);
  inline void getMultiInts(std::vector< std::vector<int> >&);
  inline void getMultiLongs(std::vector< std::vector<long> >&);
  inline void getMultiULongs(std::vector< std::vector<unsigned long> >&);
  inline void getMultiFloats(std::vector< std::vector<float> >&);
  inline void getMultiDoubles(std::vector< std::vector<double> >&);
  inline void getMultiStrings(std::vector< std::vector<std::string> >&);
  
  // defaults value regardless of being set by user.
  std::string defaults;
  // If expects arguments, this will delimit arg list.
  char delim;
  // If not 0, then number of delimited args. -1 for arbitrary number.
  int expectArgs;
  // Descriptive help message shown in usage instructions for option.
  std::string help;
  // 0 or 1.
  bool isRequired;
  // A list of flags that denote this option, i.e. -d, --dimension.
  std::vector< std::string* > flags;
  // If was set (or found).
  bool isSet;
  // Lists of arguments, per flag instance, after splitting by delimiter.
  std::vector< std::vector< std::string* > * > args;
  // Index where each group was parsed from input stream to track order.
  std::vector<int> parseIndex;
};
/* ################################################################### */
void OptionGroup::clearArgs() {
  int i,j;
  for(i=0; i < (long int)args.size(); ++i) {
    for(j=0; j < (long int)args[i]->size(); ++j)
      delete args[i]->at(j);
      
    delete args[i];
  }

  args.clear();
  isSet = false;
};
/* ################################################################### */
void OptionGroup::getInt(int & out) {
  if (!isSet) {
    if (defaults.empty())
      out = 0;
    else
      out = atoi(defaults.c_str());
  } else {
    if (args.empty() || args[0]->empty())
      out = 0;
    else {
      out = atoi(args[0]->at(0)->c_str());
    }
  }
};
/* ################################################################### */
void OptionGroup::getLong(long & out) {
  if (!isSet) {
    if (defaults.empty())
      out = 0;
    else
      out = atoi(defaults.c_str());
  } else {
    if (args.empty() || args[0]->empty())
      out = 0;
    else {
      out = atol(args[0]->at(0)->c_str());
    }
  }
};
/* ################################################################### */
void OptionGroup::getLongLong(long long & out) {
  if (!isSet) {
    if (defaults.empty())
      out = 0;
    else {
      std::stringstream ss(defaults.c_str());
      ss >> out;
    }
  } else {
    if (args.empty() || args[0]->empty())
      out = 0;
    else {
      std::stringstream ss(args[0]->at(0)->c_str());
      ss >> out;
    }
  }
};
/* ################################################################### */
void OptionGroup::getULong(unsigned long & out) {
  if (!isSet) {
    if (defaults.empty())
      out = 0;
    else
      out = atoi(defaults.c_str());
  } else {
    if (args.empty() || args[0]->empty())
      out = 0;
    else {
      out = strtoul(args[0]->at(0)->c_str(),0,0);
    }
  }
};
/* ################################################################### */
void OptionGroup::getULongLong(unsigned long long & out) {
  if (!isSet) {
    if (defaults.empty())
      out = 0;
    else {
      std::stringstream ss(defaults.c_str());
      ss >> out;
    }
  } else {
    if (args.empty() || args[0]->empty())
      out = 0;
    else {
      std::stringstream ss(args[0]->at(0)->c_str());
      ss >> out;
    }
  }
};
/* ################################################################### */
void OptionGroup::getFloat(float & out) {
  if (!isSet) {
    if (defaults.empty())
      out = 0.0;
    else
      out = (float)atof(defaults.c_str());
  } else {
    if (args.empty() || args[0]->empty())
      out = 0.0;
    else {
      out = (float)atof(args[0]->at(0)->c_str());
    }
  }
};
/* ################################################################### */
void OptionGroup::getDouble(double & out) {
  if (!isSet) {
    if (defaults.empty())
      out = 0.0;
    else
      out = atof(defaults.c_str());
  } else {
    if (args.empty() || args[0]->empty())
      out = 0.0;
    else {
      out = atof(args[0]->at(0)->c_str());
    }
  }
};
/* ################################################################### */
void OptionGroup::getString(std::string & out) {
  if (!isSet) {
    out = defaults;
  } else {
    if (args.empty() || args[0]->empty())
      out = "";
    else {
      out = *args[0]->at(0);
    }
  }
};
/* ################################################################### */
void OptionGroup::getInts(std::vector<int> & out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      StringsToInts(strings, out);
    }
  } else {
    if (!(args.empty() || args[0]->empty()))
      StringsToInts(args[0], &out);
  }
};
/* ################################################################### */
void OptionGroup::getLongs(std::vector<long> & out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      StringsToLongs(strings, out);
    }
  } else {
    if (!(args.empty() || args[0]->empty()))
      StringsToLongs(args[0], &out);
  }
};
/* ################################################################### */
void OptionGroup::getULongs(std::vector<unsigned long> & out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      StringsToULongs(strings, out);
    }
  } else {
    if (!(args.empty() || args[0]->empty()))
      StringsToULongs(args[0], &out);
  }
};
/* ################################################################### */
void OptionGroup::getFloats(std::vector<float> & out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      StringsToFloats(strings, out);
    }
  } else {
    if (!(args.empty() || args[0]->empty()))
      StringsToFloats(args[0], &out);
  }
};
/* ################################################################### */
void OptionGroup::getDoubles(std::vector<double> & out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      StringsToDoubles(strings, out);
    }
  } else {
    if (!(args.empty() || args[0]->empty()))
      StringsToDoubles(args[0], &out);
  }
};
/* ################################################################### */
void OptionGroup::getStrings(std::vector<std::string>& out) {
  if (!isSet) {
    if (!defaults.empty()) {
      SplitDelim(defaults, delim, out);
    }
  } else {
    if (!(args.empty() || args[0]->empty()))
      StringsToStrings(args[0], &out);
  }
};
/* ################################################################### */
void OptionGroup::getMultiInts(std::vector< std::vector<int> >& out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      if (out.size() < 1) out.resize(1);
      StringsToInts(strings, out[0]);
    }
  } else {
    if (!args.empty()) {
      int n = args.size();
      if ((long int)out.size() < n) out.resize(n);
      for(int i=0; i < n; ++i) {
        StringsToInts(args[i], &out[i]);
      }
    }
  }
};
/* ################################################################### */
void OptionGroup::getMultiLongs(std::vector< std::vector<long> >& out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      if (out.size() < 1) out.resize(1);
      StringsToLongs(strings, out[0]);
    }
  } else {
    if (!args.empty()) {
      int n = args.size();
      if ((long int)out.size() < n) out.resize(n);
      for(int i=0; i < n; ++i) {
        StringsToLongs(args[i], &out[i]);
      }
    }
  }
};
/* ################################################################### */
void OptionGroup::getMultiULongs(std::vector< std::vector<unsigned long> >& out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      if (out.size() < 1) out.resize(1);
      StringsToULongs(strings, out[0]);
    }
  } else {
    if (!args.empty()) {
      int n = args.size();
      if ((long int)out.size() < n) out.resize(n);
      for(int i=0; i < n; ++i) {
        StringsToULongs(args[i], &out[i]);
      }
    }
  }
};
/* ################################################################### */
void OptionGroup::getMultiFloats(std::vector< std::vector<float> >& out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      if (out.size() < 1) out.resize(1);
      StringsToFloats(strings, out[0]);
    }
  } else {
    if (!args.empty()) {
      int n = args.size();
      if ((long int)out.size() < n) out.resize(n);
      for(int i=0; i < n; ++i) {
        StringsToFloats(args[i], &out[i]);
      }
    }
  }
};
/* ################################################################### */
void OptionGroup::getMultiDoubles(std::vector< std::vector<double> >& out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      if (out.size() < 1) out.resize(1);
      StringsToDoubles(strings, out[0]);
    }
  } else {
    if (!args.empty()) {
      int n = args.size();
      if ((long int)out.size() < n) out.resize(n);
      for(int i=0; i < n; ++i) {
        StringsToDoubles(args[i], &out[i]);
      }
    }
  }
};
/* ################################################################### */
void OptionGroup::getMultiStrings(std::vector< std::vector<std::string> >& out) {
  if (!isSet) {
    if (!defaults.empty()) {
      std::vector< std::string > strings;
      SplitDelim(defaults, delim, strings);
      if (out.size() < 1) out.resize(1);
      out[0] = strings;
    }
  } else {
    if (!args.empty()) {
      int n = args.size();
      if ((long int)out.size() < n) out.resize(n);

      for(int i=0; i < n; ++i) {
        for(int j=0; j < (long int)args[i]->size(); ++j) 
          out[i].push_back( *args[i]->at(j) );
      }
    }
  }
};
/* ################################################################### */
typedef std::map< int, ezOptionValidator* > ValidatorMap;

class ezOptionParser {
public:
  // How to layout usage descriptions with the option flags.
  enum Layout { ALIGN, INTERLEAVE, STAGGER };

  inline ~ezOptionParser();
  
  inline void add(const char * defaults, bool required, int expectArgs, char delim, const char * help, const char * flag1, ezOptionValidator* validator=0);
  inline void add(const char * defaults, bool required, int expectArgs, char delim, const char * help, const char * flag1, const char * flag2, ezOptionValidator* validator=0);
  inline void add(const char * defaults, bool required, int expectArgs, char delim, const char * help, const char * flag1, const char * flag2, const char * flag3, ezOptionValidator* validator=0);
  inline void add(const char * defaults, bool required, int expectArgs, char delim, const char * help, const char * flag1, const char * flag2, const char * flag3, const char * flag4, ezOptionValidator* validator=0);
  inline bool exportFile(const char * filename, bool all=false);
  inline OptionGroup * get(const char * name);
  inline void getUsage(std::string & usage, int width=80, Layout layout=ALIGN);
  inline void getUsageDescriptions(std::string & usage, int width=80, Layout layout=STAGGER);
  inline bool gotExpected(std::vector<std::string> & badOptions);
  inline bool gotRequired(std::vector<std::string> & badOptions);
  inline bool gotValid(std::vector<std::string> & badOptions, std::vector<std::string> & badArgs);
  inline bool importFile(const char * filename, char comment='#');
  inline int isSet(const char * name);
  inline int isSet(std::string & name);
  inline void parse(int argc, const char * argv[]);
  inline void prettyPrint(std::string & out); 
  inline void reset();
  inline void resetArgs();
    
  // Insert extra empty line betwee each option's usage description.
  char doublespace;
  // General description in human language on what the user's tool does.
  // It's the first section to get printed in the full usage message.
  std::string overview;
  // A synopsis of command and options usage to show expected order of input arguments.
  // It's the second section to get printed in the full usage message.
  std::string syntax;
  // Example (third) section in usage message.
  std::string example;
  // Final section printed in usage message. For contact, copyrights, version info.
  std::string footer;
  // Map from an option to an Id of its parent group.
  std::map< std::string, int > optionGroupIds;
  // Unordered collection of the option groups.
  std::vector< OptionGroup* > groups;
  // Store unexpected args in input.
  std::vector< std::string* > unknownArgs;
  // List of args that occur left-most before first option flag.
  std::vector< std::string* > firstArgs;
  // List of args that occur after last right-most option flag and its args.
  std::vector< std::string* > lastArgs;
  // List of validators.
  ValidatorMap validators;
  // Maps group id to a validator index into vector of validators. Validator index is -1 if there is no validator for group.
  std::map< int, int > groupValidators;
};
/* ################################################################### */
ezOptionParser::~ezOptionParser() {
  reset();
}
/* ################################################################### */
void ezOptionParser::reset() {
  this->doublespace = 1;
  
  int i;
  for(i=0; i < (long int)groups.size(); ++i)
    delete groups[i];
  groups.clear();
  
  for(i=0; i < (long int)unknownArgs.size(); ++i)
    delete unknownArgs[i];
  unknownArgs.clear();
  
  for(i=0; i < (long int)firstArgs.size(); ++i)
    delete firstArgs[i];
  firstArgs.clear();

  for(i=0; i < (long int)lastArgs.size(); ++i)
    delete lastArgs[i];
  lastArgs.clear();
  
  ValidatorMap::iterator it;
  for(it = validators.begin(); it != validators.end(); ++it)
    delete it->second;
  
  validators.clear();
  optionGroupIds.clear();
  groupValidators.clear();
};
/* ################################################################### */
void ezOptionParser::resetArgs() {
  int i;
  for(i=0; i < (long int)groups.size(); ++i)
    groups[i]->clearArgs();
  
  for(i=0; i < (long int)unknownArgs.size(); ++i)
    delete unknownArgs[i];
  unknownArgs.clear();
  
  for(i=0; i < (long int)firstArgs.size(); ++i)
    delete firstArgs[i];
  firstArgs.clear();

  for(i=0; i < (long int)lastArgs.size(); ++i)
    delete lastArgs[i];
  lastArgs.clear();  
};
/* ################################################################### */
void ezOptionParser::add(const char * defaults, bool required, int expectArgs, char delim, const char * help, const char * flag1, ezOptionValidator* validator) {
  int id = this->groups.size();
  OptionGroup * g = new OptionGroup;
  g->defaults = defaults;
  g->isRequired = required;
  g->expectArgs = expectArgs;
  g->delim = delim;
  g->isSet = 0;
  g->help = help;
  std::string *f1 = new std::string(flag1);
  g->flags.push_back( f1 );
  this->optionGroupIds[flag1] = id;
  this->groups.push_back(g);

  if (validator) {
    int vid = validator->id;
    validators[vid] = validator;
    groupValidators[id] = vid;
  } else {
    groupValidators[id] = -1;
  }
};
/* ################################################################### */
void ezOptionParser::add(const char * defaults, bool required, int expectArgs, char delim, const char * help, const char * flag1, const char * flag2, ezOptionValidator* validator) {
  int id = this->groups.size();
  OptionGroup * g = new OptionGroup;
  g->defaults = defaults;
  g->isRequired = required;
  g->expectArgs = expectArgs;
  g->delim = delim;
  g->isSet = 0;
  g->help = help;
  std::string *f1 = new std::string(flag1);
  g->flags.push_back( f1 );
  std::string *f2 = new std::string(flag2);
  g->flags.push_back( f2 );
  this->optionGroupIds[flag1] = id;
  this->optionGroupIds[flag2] = id;

  this->groups.push_back(g);
  
  if (validator) {
    int vid = validator->id;
    validators[vid] = validator;
    groupValidators[id] = vid;
  } else {
    groupValidators[id] = -1;
  }
};
/* ################################################################### */
void ezOptionParser::add(const char * defaults, bool required, int expectArgs, char delim, const char * help, const char * flag1, const char * flag2, const char * flag3, ezOptionValidator* validator) {
  int id = this->groups.size();
  OptionGroup * g = new OptionGroup;
  g->defaults = defaults;
  g->isRequired = required;
  g->expectArgs = expectArgs;
  g->delim = delim;
  g->isSet = 0;
  g->help = help;
  std::string *f1 = new std::string(flag1);
  g->flags.push_back( f1 );
  std::string *f2 = new std::string(flag2);
  g->flags.push_back( f2 );
  std::string *f3 = new std::string(flag3);
  g->flags.push_back( f3 );
  this->optionGroupIds[flag1] = id;
  this->optionGroupIds[flag2] = id;
  this->optionGroupIds[flag3] = id;

  this->groups.push_back(g);
  
  if (validator) {
    int vid = validator->id;
    validators[vid] = validator;
    groupValidators[id] = vid;
  } else {
    groupValidators[id] = -1;
  }
};
/* ################################################################### */
void ezOptionParser::add(const char * defaults, bool required, int expectArgs, char delim, const char * help, const char * flag1, const char * flag2, const char * flag3, const char * flag4, ezOptionValidator* validator) {
  int id = this->groups.size();
  OptionGroup * g = new OptionGroup;
  g->defaults = defaults;
  g->isRequired = required;
  g->expectArgs = expectArgs;
  g->delim = delim;
  g->isSet = 0;
  g->help = help;
  std::string *f1 = new std::string(flag1);
  g->flags.push_back( f1 );
  std::string *f2 = new std::string(flag2);
  g->flags.push_back( f2 );
  std::string *f3 = new std::string(flag3);
  g->flags.push_back( f3 );
  std::string *f4 = new std::string(flag4);
  g->flags.push_back( f4 );
  this->optionGroupIds[flag1] = id;
  this->optionGroupIds[flag2] = id;
  this->optionGroupIds[flag3] = id;
  this->optionGroupIds[flag4] = id;

  this->groups.push_back(g);
  
  if (validator) {
    int vid = validator->id;
    validators[vid] = validator;
    groupValidators[id] = vid;
  } else {
    groupValidators[id] = -1;
  }
};
/* ################################################################### */
bool ezOptionParser::exportFile(const char * filename, bool all) {
  int i;
  std::string out;
  bool quote;
  
  // Export the first args, except the program name, so start from 1.
  for(i=1; i < (long int)firstArgs.size(); ++i) {
    quote = ((firstArgs[i]->find_first_of(" \t") != std::string::npos) && (firstArgs[i]->find_first_of("\'\"") == std::string::npos));

    if (quote)
      out.append("\"");

    out.append(*firstArgs[i]);
    if (quote)
      out.append("\"");

    out.append(" ");
  }
  
  if (firstArgs.size() > 1)
    out.append("\n");
  
  std::vector<std::string* > stringPtrs(groups.size());
  int m;
  int n = groups.size();
  for(i=0; i < n; ++i) {
    stringPtrs[i] = groups[i]->flags[0];
  }

  OptionGroup *g;
  // Sort first flag of each group with other groups.
  std::sort(stringPtrs.begin(), stringPtrs.end(), CmpOptStringPtr);
  for(i=0; i < n; ++i) {
    g = get(stringPtrs[i]->c_str());
    if (g->isSet || all) {
      if (!g->isSet || g->args.empty()) {
        if (!g->defaults.empty()) {
          out.append(*stringPtrs[i]);
          out.append(" ");
          quote = ((g->defaults.find_first_of(" \t") != std::string::npos) && (g->defaults.find_first_of("\'\"") == std::string::npos));
          if (quote)
            out.append("\"");
            
          out.append(g->defaults);
          if (quote)
            out.append("\"");
            
          out.append("\n");
        }
      } else {
        int n = g->args.size();
        for(int j=0; j < n; ++j) {
          out.append(*stringPtrs[i]);
          out.append(" ");
          m = g->args[j]->size();
          
          for(int k=0; k < m; ++k) {
            quote = ( (*g->args[j]->at(k)).find_first_of(" \t") != std::string::npos );
            if (quote)
              out.append("\"");
              
            out.append(*g->args[j]->at(k));
            if (quote)
              out.append("\"");
              
            if ((g->delim) && ((k+1) != m))
              out.append(1,g->delim);
          }
          out.append("\n");
        }
      }
    }
  }
  
  // Export the last args.
  for(i=0; i < (long int)lastArgs.size(); ++i) {
    quote = ( lastArgs[i]->find_first_of(" \t") != std::string::npos );
    if (quote)
      out.append("\"");

    out.append(*lastArgs[i]);
    if (quote)
      out.append("\"");

    out.append(" ");
  }
  
  std::ofstream file(filename);
  if (!file.is_open())
    return false;

  file << out;
  file.close();
  
  return true;
};
/* ################################################################### */
// Does not overwrite current options.
// Returns true if file was read successfully.
// So if this is used before parsing CLI, then option values will reflect
// this file, but if used after parsing CLI, then values will contain
// both CLI values and file's values.
//
// Comment lines are allowed if prefixed with #.
// Strings should be quoted as usual.
bool ezOptionParser::importFile(const char * filename, char comment) {
  std::ifstream file (filename, std::ios::in | std::ios::ate);
  if (!file.is_open())
    return false;

  // Read entire file contents.
  std::ifstream::pos_type size = file.tellg();
  char * memblock = new char[(int)size+1]; // Add one for end of string.
  file.seekg (0, std::ios::beg);
  file.read (memblock, size);
  memblock[size] = '\0';
  file.close();
  
  // Find comment lines.
  std::list<std::string*> lines;
  std::string memblockstring(memblock);
  delete[] memblock;
  SplitDelim(memblockstring, '\n', lines);
  int i,j,n;
  std::list<std::string*>::iterator iter;
  std::vector<int> sq, dq; // Single and double quote indices.
  std::vector<int>::iterator lo; // For searching quote indices.
  size_t pos;
  const char *str;
  std::string *line;
  // Find all single and double quotes to correctly handle comment tokens.
  for(iter=lines.begin(); iter != lines.end(); ++iter) {
    line = *iter;
    str = line->c_str();
    n = line->size();
    sq.clear();
    dq.clear();
    if (n) {
      // If first char is comment, then erase line and continue.
      pos = line->find_first_not_of(" \t\r");
      if ((pos==std::string::npos) || (line->at(pos)==comment)) {
        line->erase();
        continue; 
      } else {
        // Erase whitespace prefix.
        line->erase(0,pos);
        n = line->size();
      }

      if (line->at(0)=='"')
        dq.push_back(0);
    
      if (line->at(0)=='\'')
        sq.push_back(0);
    } else { // Empty line.
      continue; 
    }

    for(i=1; i < n; ++i) {
      if ( (str[i]=='"') && (str[i-1]!='\\') )
        dq.push_back(i);
      else if ( (str[i]=='\'') && (str[i-1]!='\\') )
        sq.push_back(i);
    }
    // Scan for comments, and when found, check bounds of quotes.
    // Start with second char because already checked first char.
    for(i=1; i < n; ++i) {
      if ( (line->at(i)==comment) && (line->at(i-1)!='\\') ) {
        // If within open/close quote pair, then not real comment.
        if (sq.size()) {
          lo = std::lower_bound(sq.begin(), sq.end(), i);
          // All start of strings will be even indices, closing quotes is odd indices.
          j = (int)(lo-sq.begin());
          if ( (j % 2) == 0) { // Even implies comment char not in quote pair.
            // Erase from comment char to end of line.
            line->erase(i);
            break;
          }
        }  else if (dq.size()) {
          // Repeat tests for double quotes.
          lo = std::lower_bound(dq.begin(), dq.end(), i);
          j = (int)(lo-dq.begin());
          if ( (j % 2) == 0) {
            line->erase(i);
            break;
          }
        } else {        
          // Not in quotes.
          line->erase(i);
          break;
        }
      }
    }  
  }
      
  std::string cmd;
  // Convert list to string without newlines to simulate commandline.
  for(iter=lines.begin(); iter != lines.end(); ++iter) {
    if (! (*iter)->empty()) {
      cmd.append(**iter);
      cmd.append(" ");
    }
  }
  
  // Now parse as if from command line. 
  int argc=0;
  char** argv = CommandLineToArgvA((char*)cmd.c_str(), &argc);
  
  // Parse.
  parse(argc, (const char**)argv);
  if (argv) free(argv);
  for(iter=lines.begin(); iter != lines.end(); ++iter)
    delete *iter;
    
  return true;
};
/* ################################################################### */
int ezOptionParser::isSet(const char * name) {
  std::string sname(name);
  
  if (this->optionGroupIds.count(sname)) {
    return this->groups[this->optionGroupIds[sname]]->isSet;
  }
  
  return 0;
};
/* ################################################################### */
int ezOptionParser::isSet(std::string & name) {
  if (this->optionGroupIds.count(name)) {
    return this->groups[this->optionGroupIds[name]]->isSet;
  }
  
  return 0;
};
/* ################################################################### */
OptionGroup * ezOptionParser::get(const char * name) {
  if (optionGroupIds.count(name)) {
    return groups[optionGroupIds[name]];
  }
  
  return 0;
};
/* ################################################################### */
void ezOptionParser::getUsage(std::string & usage, int width, Layout layout) {

  usage.append(overview);
  usage.append("\n\n");
  usage.append("USAGE: ");
  usage.append(syntax);
  usage.append("\n\nOPTIONS:\n\n");
  getUsageDescriptions(usage, width, layout);

  if (!example.empty()) {
    usage.append("EXAMPLES:\n\n");
    usage.append(example);
  }
  
  if (!footer.empty()) {
    usage.append(footer);
  }
};
/* ################################################################### */
// Creates 2 column formatted help descriptions for each option flag.
void ezOptionParser::getUsageDescriptions(std::string & usage, int width, Layout layout) {
  // Sort each flag list amongst each group.
  int i;
  // Store index of flag groups before sort for easy lookup later.
  std::map<std::string*, int> stringPtrToIndexMap;
  std::vector<std::string* > stringPtrs(groups.size());
  
  for(i=0; i < (long int)groups.size(); ++i) {
    std::sort(groups[i]->flags.begin(), groups[i]->flags.end(), CmpOptStringPtr);
    stringPtrToIndexMap[groups[i]->flags[0]] = i;
    stringPtrs[i] = groups[i]->flags[0];
  }
  
  size_t j, k;
  std::string opts;
  std::vector<std::string> sortedOpts;
  // Sort first flag of each group with other groups.
  std::sort(stringPtrs.begin(), stringPtrs.end(), CmpOptStringPtr);
   for(i=0; i < (long int)groups.size(); ++i) {
    //printf("DEBUG:%d: %d %d %s\n", __LINE__, i, stringPtrToIndexMap[stringPtrs[i]], stringPtrs[i]->c_str());
    k = stringPtrToIndexMap[stringPtrs[i]];
    opts.clear();
    for(j=0; j < groups[k]->flags.size()-1; ++j) {
      opts.append(*groups[k]->flags[j]);
      opts.append(", ");
      
      if ((long int)opts.size() > width)
        opts.append("\n");
    }
    // The last flag. No need to append comma anymore.
    opts.append( *groups[k]->flags[j] );
    
    if (groups[k]->expectArgs) {
      opts.append(" ARG");
      
      if (groups[k]->delim) {
        opts.append("1[");
        opts.append(1, groups[k]->delim);
        opts.append("ARGn]");
      }
    }
      
    sortedOpts.push_back(opts);
  }  
  
  // Each option group will use this to build multiline help description.
  std::list<std::string*> desc; 
  // Number of whitespaces from start of line to description (interleave layout) or
  // gap between flag names and description (align, stagger layouts).
  int gutter = 3;
  
  // Find longest opt flag string to set column start for help usage descriptions.
  int maxlen=0;
  if (layout == ALIGN) {
    for(i=0; i < (long int)groups.size(); ++i) {
      if (maxlen < (long int)sortedOpts[i].size())
        maxlen = sortedOpts[i].size();
    }  
  }
  
  // The amount of space remaining on a line for help text after flags.
  int helpwidth;
  std::list<std::string*>::iterator cIter, insertionIter;
  size_t pos;
  for(i=0; i < (long int)groups.size(); ++i) {
    k = stringPtrToIndexMap[stringPtrs[i]];
    
    if (layout == STAGGER)
      maxlen = sortedOpts[i].size();
      
    int pad = gutter + maxlen;
    helpwidth = width - pad;        
    
    // All the following split-fu could be optimized by just using substring (offset, length) tuples, but just to get it done, we'll do some not-too expensive string copying.
    SplitDelim(groups[k]->help, '\n', desc);
    // Split lines longer than allowable help width.
    for(insertionIter=desc.begin(), cIter=insertionIter++; 
        cIter != desc.end(); 
        cIter=insertionIter++) {
      if ((long int)((*cIter)->size()) > helpwidth) {
        // Get pointer to next string to insert new strings before it.
        std::string *rem = *cIter;
        // Remove this line and add back in pieces.
        desc.erase(cIter);
        // Loop until remaining string is short enough.
        while ((long int)rem->size() > helpwidth) {
          // Find whitespace to split before helpwidth.
          if (rem->at(helpwidth) == ' ') {
            // If word ends exactly at helpwidth, then split after it.
            pos = helpwidth;
          } else {
            // Otherwise, split occurs midword, so find whitespace before this word.
            pos = rem->rfind(" ", helpwidth);
          }
          // Insert split string.
          desc.insert(insertionIter, new std::string(*rem, 0, pos));
          // Now skip any whitespace to start new line.
          pos = rem->find_first_not_of(' ', pos);
          rem->erase(0, pos);
        }
        
        if (rem->size())
          desc.insert(insertionIter, rem);
        else
          delete rem;
      }
    }

    usage.append(sortedOpts[i]);
    if (layout != INTERLEAVE)
      // Add whitespace between option names and description.
      usage.append(pad - sortedOpts[i].size(), ' ');
    else {
      usage.append("\n");
      usage.append(gutter, ' ');
    }
        
    if (desc.size() > 0) { // Crash fix by Bruce Shankle.
      // First line already padded above (before calling SplitDelim) after option flag names.
      cIter = desc.begin();
      usage.append(**cIter);
      usage.append("\n");
      // Now inject the pad for each line.
      for(++cIter; cIter != desc.end(); ++cIter) {      
        usage.append(pad, ' ');
        usage.append(**cIter);
        usage.append("\n");
      }

      if (this->doublespace) usage.append("\n");    
      
      for(cIter=desc.begin(); cIter != desc.end(); ++cIter)    
        delete *cIter;
      
      desc.clear();      
    }
    
  }  
};
/* ################################################################### */
bool ezOptionParser::gotExpected(std::vector<std::string> & badOptions) {
  int i,j;
  
  for(i=0; i < (long int)groups.size(); ++i) {
    OptionGroup *g = groups[i];
    // If was set, ensure number of args is correct.
    if (g->isSet) {
      if ((g->expectArgs != 0) && g->args.empty()) {
        badOptions.push_back(*g->flags[0]);
        continue;
      }
      
      for(j=0; j < (long int)g->args.size(); ++j) {
        if ((g->expectArgs != -1) && (g->expectArgs != (long int)g->args[j]->size()))
          badOptions.push_back(*g->flags[0]);
      }
    }
  }

  return badOptions.empty();
};
/* ################################################################### */
bool ezOptionParser::gotRequired(std::vector<std::string> & badOptions) {
  int i;
  
  for(i=0; i < (long int)groups.size(); ++i) {
    OptionGroup *g = groups[i];
    // Simple case when required but user never set it.
    if (g->isRequired && (!g->isSet)) {
      badOptions.push_back(*g->flags[0]);
      continue;
    }
  }

  return badOptions.empty();
};
/* ################################################################### */
bool ezOptionParser::gotValid(std::vector<std::string> & badOptions, std::vector<std::string> & badArgs) {
  int groupid, validatorid;
  std::map< int, int >::iterator it;
  
  for(it = groupValidators.begin(); it != groupValidators.end(); ++it) {
    groupid = it->first;
    validatorid = it->second;
    if (validatorid < 0) continue;
    
    OptionGroup *g = groups[groupid];
    ezOptionValidator *v = validators[validatorid];
    bool nextgroup = false;
    
    for (int i = 0; i < (long int)g->args.size(); ++i) {
      if (nextgroup) break;
      std::vector< std::string* > * args = g->args[i];
      for (int j = 0; j < (long int)args->size(); ++j) {
        if (!v->isValid(args->at(j))) {
          badOptions.push_back(*g->flags[0]);
          badArgs.push_back(*args->at(j));
          nextgroup = true;
          break;
        }
      }
    }
  }

  return badOptions.empty();
};
/* ################################################################### */
void ezOptionParser::parse(int argc, const char * argv[]) {
  if (argc < 1) return;
  
  /*
  std::map<std::string,int>::iterator it;
  for ( it=optionGroupIds.begin() ; it != optionGroupIds.end(); it++ )
    std::cout << (*it).first << " => " << (*it).second << std::endl;
  */
  
  int i, k, firstOptIndex=0, lastOptIndex=0;
  std::string s;
  OptionGroup *g;
  
  for(i=0; i < argc; ++i) {
    s = argv[i];

    if (optionGroupIds.count(s))
      break;
  }
  
  firstOptIndex = i;

  if (firstOptIndex == argc) {
    // No flags encountered, so set last args.
    this->firstArgs.push_back(new std::string(argv[0]));

    for(k=1; k < argc; ++k)
      this->lastArgs.push_back(new std::string(argv[k]));
      
    return;
  }

  // Store initial args before opts appear.
  for(k=0; k < i; ++k) {
    this->firstArgs.push_back(new std::string(argv[k]));
  }
  
  for(; i < argc; ++i) {
    s = argv[i];
    
    if (optionGroupIds.count(s)) {
      k = optionGroupIds[s];
      g = groups[k];
      g->isSet = 1;
      g->parseIndex.push_back(i);

      if (g->expectArgs) {
        // Read ahead to get args.
        ++i;
        if (i >= argc) return;
        g->args.push_back(new std::vector<std::string*>);
        SplitDelim(argv[i], g->delim, g->args.back());
      }
      lastOptIndex = i;
    }
  }
  
  // Scan for unknown opts/arguments.
  for(i=firstOptIndex; i <= lastOptIndex; ++i) {
    s = argv[i];
    
    if (optionGroupIds.count(s)) {
      k = optionGroupIds[s];
      g = groups[k];
      if (g->expectArgs) {
        // Read ahead for args and skip them.
        ++i;
      }
    } else {
      unknownArgs.push_back(new std::string(argv[i]));
    }
  }
  
  if ( lastOptIndex >= (argc-1) ) return;
  
  // Store final args without flags.
  for(k=lastOptIndex + 1; k < argc; ++k) {
    this->lastArgs.push_back(new std::string(argv[k]));
  }
};
/* ################################################################### */
void ezOptionParser::prettyPrint(std::string & out) {
  char tmp[256];
  int i,j,k;
  
  out += "First Args:\n";
  for(i=0; i < (long int)firstArgs.size(); ++i) {
    sprintf(tmp, "%d: %s\n", i+1, firstArgs[i]->c_str());
    out += tmp;
  }

  // Sort the option flag names.
  int n = groups.size();
  std::vector<std::string* > stringPtrs(n);
  for(i=0; i < n; ++i) {
    stringPtrs[i] = groups[i]->flags[0];
  }

  // Sort first flag of each group with other groups.
  std::sort(stringPtrs.begin(), stringPtrs.end(), CmpOptStringPtr);

  out += "\nOptions:\n";
  OptionGroup *g;
  for(i=0; i < n; ++i) {            
    g = get(stringPtrs[i]->c_str());
    out += "\n";
    // The flag names:
    for(j=0; j < (long int)g->flags.size()-1; ++j) {
      sprintf(tmp, "%s, ", g->flags[j]->c_str());
      out += tmp;
    }
    sprintf(tmp, "%s:\n", g->flags.back()->c_str());
    out += tmp;

    if (g->isSet) {
      if (g->expectArgs) {
        if (g->args.empty()) {
          sprintf(tmp, "%s (default)\n", g->defaults.c_str());
          out += tmp;
        } else {
          for(k=0; k < (long int)g->args.size(); ++k) {            
            for(j=0; j < (long int)g->args[k]->size()-1; ++j) {
              sprintf(tmp, "%s%c", g->args[k]->at(j)->c_str(), g->delim);
              out += tmp;
            }
            sprintf(tmp, "%s\n", g->args[k]->back()->c_str());
            out += tmp;
          }
        }
      } else { // Set but no args expected.
        sprintf(tmp, "Set\n");
        out += tmp;
      }
    } else {
      sprintf(tmp, "Not set\n");
      out += tmp;
    }
  }
  
  out += "\nLast Args:\n";
  for(i=0; i < (long int)lastArgs.size(); ++i) {
    sprintf(tmp, "%d: %s\n", i+1, lastArgs[i]->c_str());
    out += tmp;
  }
  
  out += "\nUnknown Args:\n";
  for(i=0; i < (long int)unknownArgs.size(); ++i) {
    sprintf(tmp, "%d: %s\n", i+1, unknownArgs[i]->c_str());
    out += tmp;
  }
};
}
/* ################################################################### */
#endif /* EZ_OPTION_PARSER_H */
