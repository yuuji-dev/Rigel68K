/***********************************************************************

 		STRUCASM.CPP
  This file contains the routines to assemble structured code.

   Author: Charles Kelly,
           Monroe County Community College
           http://www.monroeccc.edu/ckelly

  Modified:
          2023/12/16 Mouloud Agaoua

 ************************************************************************/
#include <algorithm>
#include <stack>
#include <vector>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "asm.h"

extern char line[256];		// Source line
extern bool listFlag;
extern bool pass2;		// Flag set during second pass
extern int loc;		// The assembler's location counter
extern unsigned int stcLabelI;  // structured if label number
extern unsigned int stcLabelW;  // structured while label number
extern unsigned int stcLabelR;  // structured repeat label number
extern unsigned int stcLabelF;  // structured for label number
extern unsigned int stcLabelD;  // structured dbloop label number
extern int errorCount, warningCount;
extern bool SEXflag;            // true expands structured listing
extern int lineNum;
extern FILE *listFile;		// Listing file
extern bool skipList;           // true to skip listing line in ASSEMBLE.CPP
extern int  macroNestLevel;     // used by macro processing
extern char lineIdent[];        // "s" used to identify structure in listing


// Define the uppercase function for use with transform
std::string toUpperCase(const std::string& str) {
  std::string result;
  std::transform(str.begin(), str.end(), std::back_inserter(result), ::toupper);
  return result;
}


// prototypes
std::string getBcc(std::string cc, int mode, int _or);
void outCmpBcc(char *size, char *op1, char *cc, char *op2, char *op3,
                    std::string label, int &error);
void assembleStc(char* line);

const unsigned int stcMask  = 0xF0000000;
const unsigned int stcMaskI = 0x00000000;
const unsigned int stcMaskW = 0x10000000;
const unsigned int stcMaskF = 0x20000000;
const unsigned int stcMaskR = 0x30000000;
const unsigned int stcMaskD = 0x40000000;

// constants for use with BccCodes[] below to select proper column.
const int RN_EA_OR = 2;
const int EA_IM_OR = 2;
const int IF_CC = 1;            // IF <cc>
const int RN_EA = 1;
const int EA_IM = 1;
const int EA_RN = 3;
const int IM_EA = 3;
const int EA_RN_OR = 4;
const int IM_EA_OR = 4;

const int BCC_COUNT = 16;
const int LAST_TOKEN = 11;      // highest token possible of structure

// Global variables
std::string stcLabel;

// Make a stack using a vector containers
std::stack<int,std::vector<int> > stcStack;
// Make a stack for saving dbloop register number
std::stack<char, std::vector<char> > dbStack;
// Make a stack for saving FOR arguments
std::stack<std::string, std::vector<std::string> > forStack;

// This table contains the branch condition codes to use for the different
// conditional expressions.
const char* BccCodes[BCC_COUNT][5] = {
//  <cc>     <-- original CC used in structured code
//            Rn<cc>ea   Rn<cc>ea OR   ea<cc>Rn    ea<cc>Rn OR  <-- if used like this
//           CMP ea,Rn    CMP ea,Rn   CMP ea,Rn    CMP ea,Rn    <-- use this CMP operation
//            ea<cc>#n   ea<cc>#n OR   #n<cc>ea    #n<cc>ea OR  <-- if used like this
//           CMP #n,ea    CMP #n,ea   CMP #n,ea    CMP #n,ea    <-- use this CMP operation
   "<GT>",      "BLE",      "BGT",      "BGE",      "BLT",    //<-- with condition below
   "<GE>",      "BLT",      "BGE",      "BGT",      "BLE",
   "<LT>",      "BGE",      "BLT",      "BLE",      "BGT",
   "<LE>",      "BGT",      "BLE",      "BLT",      "BGE",
   "<EQ>",      "BNE",      "BEQ",      "BNE",      "BEQ",
   "<NE>",      "BEQ",      "BNE",      "BEQ",      "BNE",
   "<HI>",      "BLS",      "BHI",      "BHS",      "BLO",
   "<HS>",      "BLO",      "BHS",      "BHI",      "BLS",
   "<CC>",      "BLO",      "BCC",      "BHI",      "BLS",
   "<LO>",      "BHS",      "BLO",      "BLS",      "BHI",
   "<CS>",      "BHS",      "BCS",      "BLS",      "BHI",
   "<LS>",      "BHI",      "BLS",      "BLO",      "BHS",
   "<MI>",      "BPL",      "BMI",      "BPL",      "BMI",
   "<PL>",      "BMI",      "BPL",      "BMI",      "BPL",
   "<VC>",      "BVS",      "BVC",      "BVS",      "BVC",
   "<VS>",      "BVC",      "BVS",      "BVC",      "BVS"


};

//-------------------------------------------------------
// returns a branch instruction
 
// or is 1 on ea <cc> ea OR, 0 otherwise
std::string getBcc(std::string cc, int mode, int _or) {
  cc = toUpperCase(cc);
  for (int i=0; i<BCC_COUNT; i++) {
    if (cc == BccCodes[i][0])
      return BccCodes[i][mode + _or];
  }
  return "B??";
}

//-------------------------------------------------------
// output a CMP and Branch to perform the specified expression
// Pre: the code is in all caps
// token[0] is the first token in the structure following the keyword. Normally
// the size. (IF.B  token[0] would be .B). The size may be missing in which case
// token[0] would contain the first operand or <cc> in the structure.
//    token number
//     0     1     2     3     4     5     6     7     8     9
//    <cc>  THEN
//    <cc>   OR   .B   <cc>  THEN
//    D1   <cc>   D2    OR    .B    <cc>  THEN
//    .B   <cc>  THEN
//    .B   <cc>   OR   <cc>  THEN
//    .B   <cc>   OR    .B   <cc>  THEN
//    .B    D0   <cc>   D1   THEN
//    .B   <cc>   AND   .B    D0   <cc>   D1   THEN
//    .B    D0   <cc>   D1    AND   .B    D2   <cc>   D3   THEN

//void outCmpBcc( char *size, char *op1, char *cc, char *op2, char *op3, char *last, std::string label, int &error) {
void outCmpBcc( char *token[], char *last, std::string label, int &error) {

  std::string stcLine, stcCmp, extent;
  int _or=0, n=0;

  try {
    error = OK;
    if (token[n][0] == '.') {
      if (token[n][1] == 'B')
        stcCmp = "\tCMP.B\t";
      else if (token[n][1] == 'W')
        stcCmp = "\tCMP.W\t";
      else if (token[n][1] == 'L')
        stcCmp = "\tCMP.L\t";
      else {
        error = SYNTAX;
        return;
      }
      n++;                        // token[n] at 1
    } else
      stcCmp = "\tCMP.W\t";

    // determine size of extent if present
    if (last[0] == '.') {
      if (last[1] == 'S')
        extent = ".S\t";
      else if (last[1] == 'L')
        extent = ".L\t";
      else {
        error = SYNTAX;
        return;
      }
    } else
      extent = "\t";

    if ( !(strcmp(token[n+1], "OR")) || !(strcmp(token[n+3], "OR"))) {
      _or = 1;
      extent = ".S\t";            // first branch with OR logic is always short
    }

    if (token[n][0] == '<') {     // IF <cc> THEN
      stcLine = "\t" + getBcc(token[n],IF_CC,_or) + extent + label + "\n";
      assembleStc(stcLine.data());
    }else if (token[n][0] == '#') {                    // #nn <cc> ea
      stcLine = stcCmp + (std::string)token[n] + "," + token[n+2] + "\n";
      assembleStc(stcLine.data());
      stcLine = "\t" + getBcc(token[n+1],IM_EA,_or) + extent + label + "\n";
      assembleStc(stcLine.data());
    }else if (token[n+2][0] == '#') {                    // ea <cc> #nn
      stcLine = stcCmp + (std::string)token[n+2] + "," + (std::string)token[n] + "\n";
      assembleStc(stcLine.data());
      stcLine = "\t" + getBcc(token[n+1],EA_IM,_or) + extent + label + "\n";
      assembleStc(stcLine.data());
    // Rn <cc> ea
    }else if ((token[n][0]=='A' || token[n][0]=='D') &&
               isRegNum(token[n][1])) {
      stcLine = stcCmp + (std::string)token[n+2] + "," + (std::string)token[n] + "\n";
      assembleStc(stcLine.data());
      stcLine = "\t" + getBcc(token[n+1],RN_EA,_or) + extent + label + "\n";
      assembleStc(stcLine.data());
    // ea <cc> Rn
    }else if ((token[n+2][0]=='A' || token[n+2][0]=='D') &&
               isRegNum(token[n+2][1])) {
      stcLine = stcCmp + (std::string)token[n] + "," + (std::string)token[n+2] + "\n";
      assembleStc(stcLine.data());
      stcLine = "\t" + getBcc(token[n+1],EA_RN,_or) + extent + label + "\n";
      assembleStc(stcLine.data());
    // (An)+ <cc> (An)+  also supports (SP)+ (MUST BE LAST IN IF-ELSE CHAIN)
    }else if ((token[n][0]=='(' && token[n][3]==')' && token[n][4]=='+')) {
      stcLine = stcCmp + (std::string)token[n] + "," + (std::string)token[n+2] + "\n";
      assembleStc(stcLine.data());
      stcLine = "\t" + getBcc(token[n+1],RN_EA,_or) + extent + label + "\n";
      assembleStc(stcLine.data());
    }else{
      error = SYNTAX;
    }
  }catch(...){
    error = SYNTAX;
  }
}


//--------------------------------------------------------
/*
  Structured statements
    items in brackets [] are optional.

    An expression consists of either one of the following:
      <cc>
      op1 <cc> op2

    IF expression THEN[.S|.L]
    IF[.B|.W|.L] expression THEN[.S|.L]
    IF[.B|.W|.L] expression OR[.B|.W|.L]  expression THEN[.S|.L]
    IF[.B|.W|.L] expression AND[.B|.W|.L] expression THEN[.S|.L]

    ELSE[.S|.L]

    ENDI

    WHILE expression DO[.S|.L]
    WHILE[.B|.W|.L] expression DO[.S|.L]
    WHILE[.B|.W|.L] expression OR[.B|.W|.L]  expression DO[.S|.L]
    WHILE[.B|.W|.L] expression AND[.B|.W|.L] expression DO[.S|.L]

    ENDW

    REPEAT

    UNTIL expression [DO[.S|.L]]
    UNTIL[.B|.W|.L] expression [DO[.S|.L]]
    UNTIL[.B|.W|.L] expression OR[.B|.W|.L]  expression [DO[.S|.L]]
    UNTIL[.B|.W|.L] expression AND[.B|.W|.L] expression [DO[.S|.L]]

    FOR[.B|.W|.L] op1 = op2 TO     op3        DO[.S|.L]
    FOR[.B|.W|.L] op1 = op2 TO     op3 BY op3 DO[.S|.L]
    FOR[.B|.W|.L] op1 = op2 DOWNTO op3        DO[.S|.L]
    FOR[.B|.W|.L] op1 = op2 DOWNTO op3 BY op3 DO[.S|.L]

    ENDF

    DBLOOP op1 = op2
    UNLESS
    UNLESS <F>
    UNLESS[.B|.W|.L] expression

    token number
     1     2     3     4     5     6     7     8     9    10    11    12
    IF   <cc>  THEN
    IF   <cc>   OR    .B   <cc>  THEN
    IF    .B   <cc>   THEN
    IF    .B   <cc>   OR   <cc>  THEN
    IF    .B   <cc>   OR    .B   <cc>  THEN
    IF    .B    D0    <cc>  D1   THEN
    IF    .B   <cc>   AND   .B    D0   <cc>   D1    THEN
    IF    D0   <cc>   D1    OR   <cc>  THEN
    IF    .B    D0    <cc>  D1    OR   <cc>  THEN
    IF    .B    D0    <cc>  D1    AND   .B    D2    <cc>  D3   THEN   .S
*/
int asmStructure(int size, char *label, char *arg, int *errorPtr)
{
  try {

    char *token[256];             // pointers to tokens
    char tokens[512];             // place tokens here
    char capLine[256];
    char tokenEnd[10];            // last token of structure goes here
    std::string stcLabel, stcLabel2, stcLine, sizeStr, extent;
    symbolDef *symbol;
    int error;
    int value;
    bool backRef;
    int n = 2;                    // token index
    int i;

    if (*label)                           // if label
      define(label, loc, pass2, true, errorPtr); // define label

    strcap(capLine, line);                // capitalize line
    error = OK;
    if (pass2 && listFlag) {
      if (!(macroNestLevel > 0 && skipList == true)) // if not called from macro with listing off
      {
        listLoc();
        if (macroNestLevel > 0)             // if called from macro
          listLine(line, lineIdent);        // tag line as macro
        else
          listLine(line, "\0");
      }
    }
    std::string temptext =  ". \t\n";
    tokenize(capLine,temptext.data() , token, tokens);  	// tokenize statement

    if (token[n][0] == '.')
      n = 3;

    // -------------------- IF --------------------
    // IF[.B|.W|.L] op1 <cc> op2 [OR/AND[.B|.W|.L]  op3 <cc> op4] THEN
    if (!(strcasecmp(token[1], "IF"))) {     
      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", stcLabelI);
      std::string stcLabel = "_" + std::string(buffer);

      
       // IF ?
      
  
      tokenEnd[0] = '\0';
      for (i=3; i<=LAST_TOKEN; i++) {
        if (!(strcmp(token[i], "THEN"))) {    // find THEN
          strncpy(tokenEnd, token[i+1],3);    // copy branch distance to tokenEnd
          break;
        }
      }
      if(i > LAST_TOKEN) {                 // if THEN not found
        error = THEN_EXPECTED;
        NEWERROR(*errorPtr, error);
      }
      //           .B/W/L       op1       <cc>       op2   THEN/OR/AND  THEN.?   label
      outCmpBcc(&token[2], tokenEnd, stcLabel, error);
      NEWERROR(*errorPtr, error);
      if (!(strcmp(token[n+1], "OR"))) {    // IF <cc> OR
        stcLabel2 = stcLabel;
        stcLabelI++;
        char buffer[9]; // 8 digits + null terminator
        snprintf(buffer, 9, "%08X", stcLabelI);
        std::string stcLabel = "_" + std::string(buffer);
        //           .B/W/L       op3      <cc>      op4       THEN      THEN.?    label
        outCmpBcc(&token[n+2], tokenEnd, stcLabel, error);
        NEWERROR(*errorPtr, error);
        stcLine = stcLabel2 + "\n";
        assembleStc(stcLine.data());
      } else if (!(strcmp(token[n+3], "OR"))) { // IF ea <cc> ea OR
        stcLabel2 = stcLabel;
        stcLabelI++;
        char buffer[9]; // 8 digits + null terminator
        snprintf(buffer, 9, "%08X", stcLabelI);
        std::string stcLabel = "_" + std::string(buffer);
        //           .B/W/L       op3      <cc>      op4       THEN      THEN.?    label
        outCmpBcc(&token[n+4], tokenEnd, stcLabel, error);
        NEWERROR(*errorPtr, error);
        stcLine = stcLabel2 + "\n";
        assembleStc(stcLine.data());
      } else if (!(strcmp(token[n+1], "AND"))) { // IF <cc> AND
        //            .B/W/L       op3       <cc>      op4       THEN     THEN.?    label
        outCmpBcc(&token[n+2], tokenEnd, stcLabel, error);
        NEWERROR(*errorPtr, error);
      } else if (!(strcmp(token[n+3], "AND"))) { // IF ea <cc> ea AND
        //            .B/W/L       op3       <cc>      op4       THEN     THEN.?    label
        outCmpBcc(&token[n+4], tokenEnd, stcLabel, error);
        NEWERROR(*errorPtr, error);
      }

      stcStack.push(stcLabelI);
      stcLabelI++;                        // prepare label for next if
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- ELSE --------------------
    if (!(strcmp(token[1], "ELSE"))) {
      unsigned int elseLbl = stcStack.top();
      stcStack.pop();
      if ((elseLbl & stcMask) != stcMaskI)
        NEWERROR(*errorPtr, NO_IF);
      // determine size of extent
      if (token[2][0] == '.') {
        if (token[2][1] == 'S')
          extent = ".S\t";
        else if (token[2][1] == 'L')
          extent = ".L\t";
        else {
          NEWERROR(*errorPtr, SYNTAX);
        }
      } else {
        extent = "\t";
      }

      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", stcLabelI);
      std::string stcLabel = "_" + std::string(buffer);
      stcLine = "\tBRA" + extent + stcLabel + "\n";
      assembleStc(stcLine.data());
      stcStack.push(stcLabelI);
      stcLabelI++;

      snprintf(buffer, 9, "%08X", elseLbl);
      stcLine = "_" + std::string(buffer) + "\n";

    
      assembleStc(stcLine.data());
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- ENDI --------------------
    if (!(strcmp(token[1], "ENDI"))) {
      unsigned int endiLbl = stcStack.top();
      stcStack.pop();
      if ((endiLbl & stcMask) != stcMaskI)        // if label is not from an IF
        NEWERROR(*errorPtr, NO_IF);
      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", endiLbl);
      std::string stcLine = "_" + std::string(buffer) + "\n";
      assembleStc(stcLine.data());
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- WHILE --------------------
    // WHILE[.B|.W|.L] op1 <cc> op2 [OR/AND[.B|.W|.L]  op3 <cc> op4] DO
    // WHILE <T> D0 create infinite loop
    if (!(strcmp(token[1], "WHILE"))) {          // WHILE

      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", stcLabelW);
      stcLabel = "_" + std::string(buffer);

      stcLine = stcLabel + "\n";
      assembleStc(stcLine.data());
      stcStack.push(stcLabelW);
      stcLabelW++;

      snprintf(buffer, 9, "%08X", stcLabelW);
      stcLabel = "_" + std::string(buffer);

      tokenEnd[0] = '\0';
      for (i=3; i<=LAST_TOKEN; i++) {
        if (!(strcmp(token[i], "DO"))) {     // if DO
          strncpy(tokenEnd, token[i+1], 3);  // copy branch distance to tokenEnd
          break;
        }
      }
      if(i > LAST_TOKEN)                  // if DO not found
        NEWERROR(*errorPtr, DO_EXPECTED);
      if ((strcmp(token[n], "<T>"))) {       // if not infinite loop <T>
        //            .B/W/L      op1       <cc>       op2   DO/OR/AND    DO.?     label
        outCmpBcc(&token[2], tokenEnd, stcLabel, error);
        NEWERROR(*errorPtr, error);
        if (!(strcmp(token[n+1], "OR"))) { // WHILE <cc> OR
          stcLabel2 = stcLabel;
          stcLabelW++;
          char buffer[9]; // 8 digits + null terminator
          snprintf(buffer, 9, "%08X", stcLabelW);
          std::string stcLabel = "_" + std::string(buffer);
          //           .B/W/L        op3      <cc>      op4       DO       DO.?      label
          outCmpBcc(&token[n+2], tokenEnd, stcLabel, error);
          NEWERROR(*errorPtr, error);
          stcLine = stcLabel2 + "\n";
          assembleStc(stcLine.data());
        } else if (!(strcmp(token[n+3], "OR"))) { // WHILE ea <cc> ea OR
          stcLabel2 = stcLabel;
          stcLabelW++;
          char buffer[9]; // 8 digits + null terminator
          snprintf(buffer, 9, "%08X", stcLabelW);
          std::string stcLabel = "_" + std::string(buffer);
          //           .B/W/L        op3      <cc>      op4       DO       DO.?      label
          outCmpBcc(&token[n+4], tokenEnd, stcLabel, error);
          NEWERROR(*errorPtr, error);
          stcLine = stcLabel2 + "\n";
          assembleStc(stcLine.data());
        } else if (!(strcmp(token[n+1], "AND"))) { // WHILE <cc> AND
          //           .B/W/L       op3       <cc>      op4       DO       DO.?      label
          outCmpBcc(&token[n+2], tokenEnd, stcLabel, error);
          NEWERROR(*errorPtr, error);
        } else if (!(strcmp(token[n+3], "AND"))) { // WHILE ea <cc> ea AND
          //           .B/W/L       op3       <cc>      op4       DO       DO.?      label
          outCmpBcc(&token[n+4], tokenEnd, stcLabel, error);
          NEWERROR(*errorPtr, error);
        }
      }

      stcStack.push(stcLabelW);
      stcLabelW++;
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- ENDW --------------------
    if (!(strcmp(token[1], "ENDW"))) {
      unsigned int endwLbl = stcStack.top();
      stcStack.pop();
      if ((endwLbl & stcMask) != stcMaskW)        // if label is not from a WHILE
        NEWERROR(*errorPtr, NO_WHILE);
      unsigned int whileLbl = stcStack.top();
      stcStack.pop();

      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", whileLbl);
      std::string stcLine = "\tBRA\t_" + std::string(buffer) + "\n";
      assembleStc(stcLine.data());

      snprintf(buffer, 9, "%08X", endwLbl);
      stcLine = "_" + std::string(buffer) + "\n";

      
      assembleStc(stcLine.data());
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- REPEAT --------------------
    if (!(strcmp(token[1], "REPEAT"))) {

      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", stcLabelR);
      std::string stcLabel = "_" + std::string(buffer) + "\n";

      stcLine = stcLabel + "\n";
      assembleStc(stcLine.data());
      stcStack.push(stcLabelR);
      stcLabelR++;
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- UNTIL --------------------
    // UNTIL[.B|.W|.L] op1 <cc> op2 [OR/AND[.B|.W|.L]  op3 <cc> op4] DO
    if (!(strcmp(token[1], "UNTIL"))) {          // UNTIL
      unsigned int untilLbl = stcStack.top();
      stcStack.pop();
      if ((untilLbl & stcMask) != stcMaskR)       // if label is not from a REPEAT
        NEWERROR(*errorPtr, NO_REPEAT);

        
      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", untilLbl);
      std::string stcLabel2 = "_" + std::string(buffer);



      snprintf(buffer, 9, "%08X", stcLabelR);
      stcLabel = "_" + std::string(buffer);
      

      tokenEnd[0] = '\0';
      for (i=3; i<=LAST_TOKEN; i++) {
        if (!(strcmp(token[i], "DO"))) {     // if DO
          strncpy(tokenEnd, token[i+1], 3);         // copy branch distance to tokenEnd
          break;
        }
      }
      if(i > LAST_TOKEN)                   // if DO not found
        NEWERROR(*errorPtr, DO_EXPECTED);
      if (!(strcmp(token[n+1], "OR"))) {      // UNTIL <cc> OR
        //           .B/W/L       op1       <cc>       op2   DO/OR/AND    DO.?     label
        outCmpBcc(&token[2], tokenEnd, stcLabel, error);
        NEWERROR(*errorPtr, error);
        //           .B/W/L       op3      <cc>      op4       DO         DO.?     label
        outCmpBcc(&token[n+2], tokenEnd, stcLabel2, error);
        stcLine = stcLabel + "\n";                // output label for first OR branch
        assembleStc(stcLine.data());
        stcLabelR++;
        NEWERROR(*errorPtr, error);

      } else if (!(strcmp(token[n+3], "OR"))) {      // UNTIL ea <cc> ea OR
        //           .B/W/L       op1       <cc>       op2   DO/OR/AND    DO.?     label
        outCmpBcc(&token[2], tokenEnd, stcLabel, error);
        NEWERROR(*errorPtr, error);
        //           .B/W/L       op3      <cc>      op4       DO         DO.?     label
        outCmpBcc(&token[n+4], tokenEnd, stcLabel2, error);
        stcLine = stcLabel + "\n";                // output label for first OR branch
        assembleStc(stcLine.data());
        stcLabelR++;
        NEWERROR(*errorPtr, error);

      } else {
        //           .B/W/L       op1       <cc>       op2   DO/OR/AND    DO.?     label
        outCmpBcc(&token[2], tokenEnd, stcLabel2, error);
        NEWERROR(*errorPtr, error);
        if (!(strcmp(token[n+1], "AND"))) {   // UNTIL <cc> AND
          //           .B/W/L       op3      <cc>      op4       DO         DO.?     label
          outCmpBcc(&token[n+2], tokenEnd, stcLabel2, error);
          NEWERROR(*errorPtr, error);
        } else if (!(strcmp(token[n+3], "AND"))) {    // UNTIL ea <cc> ea AND
          //           .B/W/L       op3      <cc>      op4       DO         DO.?     label
          outCmpBcc(&token[n+4], tokenEnd, stcLabel2, error);
          NEWERROR(*errorPtr, error);
        }
      }

      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- FOR --------------------
    // FOR[.<size>] op1 = op2 TO op3 [BY op4] DO
    if (!(strcmp(token[1], "FOR"))) {

      // determine size of extent if present
      tokenEnd[0] = '\0';
      for (i=3; i<=LAST_TOKEN; i++) {
        if (!(strcmp(token[i], "DO"))) {   // find DO
          strncpy(tokenEnd, token[i+1],3); // copy branch distance to tokenEnd
          break;
        }
      }
      if(i > LAST_TOKEN)                   // if DO not found
        NEWERROR(*errorPtr, DO_EXPECTED);
      if (tokenEnd[0] == '.') {
        if (tokenEnd[1] == 'S')
          extent = ".S\t";
        else if (tokenEnd[1] == 'L')
          extent = ".L\t";
        else {
          NEWERROR(*errorPtr, SYNTAX);
        }
      } else {
        extent = "\t";
      }

      // determine size of CMP
      if (token[2][0] == '.') {
        if (token[2][1] == 'B')
          sizeStr = ".B\t";
        else if (token[2][1] == 'W')
          sizeStr = ".W\t";
        else if (token[2][1] == 'L')
          sizeStr = ".L\t";
        else {
          NEWERROR(*errorPtr, SYNTAX);
        }
      } else
        sizeStr = ".W\t";

      stcLine = "\tMOVE" + sizeStr + (std::string)token[n+2] + "," + (std::string)token[n] + "\n";
      if ((strcmp(token[n+2],token[n])))  // if op1 != op2 (FOR D1 = D1 TO ... skips move)
        assembleStc(stcLine.data());     // MOVE op2,op1

      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", stcLabelF);
      std::string stcLabel = "_" + std::string(buffer);

      
      stcLabelF++;

      snprintf(buffer, 9, "%08X", stcLabelF);
      std::string stcLabel2 = "_" + std::string(buffer);

      stcLine = "\tBRA" + extent + stcLabel2 + "\n";
      assembleStc(stcLine.data());       //   BRA _20000001
      stcStack.push(stcLabelF);           // push _20000001

      stcLine = stcLabel + "\n";
      assembleStc(stcLine.data());       // _20000000

      if (!(strcmp(token[n+3], "DOWNTO")))
        stcLine = "\tBGE" + extent + stcLabel + "\n";
      else
        stcLine = "\tBLE" + extent + stcLabel + "\n";
      forStack.push(stcLine);             // push Bcc _20000000

      stcLine = "\tCMP" + sizeStr + (std::string)token[n+4] + "," + (std::string)token[n] + "\n";
      forStack.push(stcLine);             // push CMP instruction

      if (!(strcmp(token[n+5], "BY")))
        if (!(strcmp(token[n+3], "DOWNTO")))
          stcLine = "\tSUB" + sizeStr + (std::string)token[n+6] + "," + (std::string)token[n] + "\n";
        else
          stcLine = "\tADD" + sizeStr + (std::string)token[n+6] + "," + (std::string)token[n] + "\n";
      else
        if (!(strcmp(token[n+3], "DOWNTO")))
          stcLine = "\tSUB" + sizeStr + "#1," + (std::string)token[n] + "\n";
        else
          stcLine = "\tADD" + sizeStr + "#1," + (std::string)token[n] + "\n";
      forStack.push(stcLine);             // push SUB/ADD instruction

      stcLabelF++;                        // ready for next For instruction
      skipList = true;                    // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- ENDF --------------------
    if (!(strcmp(token[1], "ENDF"))) {
      unsigned int endfLbl = stcStack.top();
      stcStack.pop();
      if ((endfLbl & stcMask) != stcMaskF)  // if label is not from a FOR
        NEWERROR(*errorPtr, NO_FOR);
      else {
        stcLine = forStack.top();
        assembleStc(stcLine.data());       //   ADD|SUB op4,op1  or  ADD|SUB #1,op1
        forStack.pop();


        char buffer[9]; // 8 digits + null terminator
        snprintf(buffer, 9, "%08X", endfLbl);
        std::string stcLine = "_" + std::string(buffer);

        assembleStc(stcLine.data());       // _20000001

        stcLine = forStack.top();
        assembleStc(stcLine.data());       //   CMP op3,op1
        forStack.pop();

        stcLine = forStack.top();
        assembleStc(stcLine.data());       //   BLT .2  or  BGT .2
        forStack.pop();
      }
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- DBLOOP --------------------
    // DBLOOP op1 = op2
    if (!(strcmp(token[1], "DBLOOP"))) {
      if (token[2][0] != 'D')
        NEWERROR(*errorPtr, SYNTAX);      // syntax must be DBLOOP Dn =
      if (token[2][1] < '0' || token[2][1] > '9' || token[3][0] != '=')
        NEWERROR(*errorPtr, SYNTAX);      // syntax must be DBLOOP Dn =
      dbStack.push(token[2][1]);          // push Dn number
      stcLine = "\tMOVE\t" + (std::string)token[4] + "," + (std::string)token[2] + "\n";
      if ((strcmp(token[2],token[4])))    // if op1 != op2 (DBLOOP D0 = D0 ... skips move)
        assembleStc(stcLine.data());     //   MOVE op2,op1

      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", stcLabelD);
      std::string stcLabel = "_" + std::string(buffer);

      stcLine = stcLabel + "\n";
      assembleStc(stcLine.data());
      stcStack.push(stcLabelD);
      stcLabelD++;
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }

    // -------------------- UNLESS --------------------
    // UNLESS[.B|.W|.L] op1 <cc> op2]
    if (!(strcmp(token[1], "UNLESS"))) {     // UNLESS
      unsigned int unlessLbl = stcStack.top();
      stcStack.pop();
      if ((unlessLbl & stcMask) != stcMaskD)       // if label is not from a DBLOOP
        NEWERROR(*errorPtr, NO_DBLOOP);
      
      char buffer[9]; // 8 digits + null terminator
      snprintf(buffer, 9, "%08X", unlessLbl);
      
      std::stringstream stcLabelsus;
      stcLabelsus << "\tD" << dbStack.top() << ",_" << std::setw(8) << std::setfill('0') << std::hex << unlessLbl;

      std::string stcLabel = stcLabelsus.str();

      
      dbStack.pop();

      // UNLESS <F> and UNLESS use DBRA
      if ( !(strcmp(token[n], "<F>")) || token[2][0] == '\0') {
        stcLine = "\tDBRA" + stcLabel + "\n";
        assembleStc(stcLine.data());
      } else {
        // determine size of CMP
        if (token[2][0] == '.') {
          if (token[2][1] == 'B')
            sizeStr = ".B\t";
          else if (token[2][1] == 'W')
            sizeStr = ".W\t";
          else if (token[2][1] == 'L')
            sizeStr = ".L\t";
          else
            NEWERROR(*errorPtr, SYNTAX);
        } else
          sizeStr = ".W\t";

        if (token[n][0] == '<') {                      // UNLESS <cc>
          stcLine = "\tD" + getBcc(token[n],IF_CC,0) + stcLabel + "\n";
          assembleStc(stcLine.data());
        }else if (token[n][0] == '#') {                // UNLESS #nn <cc> ea
          stcLine = "\tCMP" + sizeStr + (std::string)token[n] + "," + (std::string)token[n+2] + "\n";
          assembleStc(stcLine.data());
          stcLine = "\tD" + getBcc(token[n+1],IM_EA,0) + stcLabel + "\n";
          assembleStc(stcLine.data());
        }else if (token[n+2][0] == '#') {                // UNLESS ea <cc> #nn
          stcLine = "\tCMP" + sizeStr + (std::string)token[n+2] + "," + (std::string)token[n] + "\n";
          assembleStc(stcLine.data());
          stcLine = "\tD" + getBcc(token[n+1],EA_IM,0) + stcLabel + "\n";
          assembleStc(stcLine.data());
        // UNLESS Rn <cc> ea
        }else if ((token[n][0]=='A' || token[n][0]=='D') && isRegNum(token[n][1])) {
          stcLine = "\tCMP" + sizeStr + (std::string)token[n+2] + "," + (std::string)token[n] + "\n";
          assembleStc(stcLine.data());
          stcLine = "\tD" + getBcc(token[n+1],RN_EA,0) + stcLabel + "\n";
          assembleStc(stcLine.data());
        // UNLESS ea <cc> Rn
        }else if ((token[n+2][0]=='A' || token[n+2][0]=='D') && isRegNum(token[n+2][1])) {
          stcLine = "\tCMP" + sizeStr + (std::string)token[n] + "," + (std::string)token[n+2] + "\n";
          assembleStc(stcLine.data());
          stcLine = "\tD" + getBcc(token[n+1],EA_RN,0) + stcLabel + "\n";
          assembleStc(stcLine.data());
        }else{
          NEWERROR(*errorPtr, SYNTAX);
        }
      }
      skipList = true;                        // don't display this line in ASSEMBLE.CPP
    }
  }catch(...){
    NEWERROR(*errorPtr, SYNTAX);
  }
  return NORMAL;
}

void assembleStc(char* line)
{
  std::string tempLine;
  int error = OK;
  int i=0;
  while(lineIdent[i] && i<MACRO_NEST_LIMIT)
    i++;
  lineIdent[i]='s';     // line identifier for listing
  lineIdent[i+1]='\0';
  if (!SEXflag)
    skipList = true;
  else
    if (!(macroNestLevel > 0 && skipList == true)) // if not called from macro with listing off
      skipList = false;
  assemble(line, &error);
  lineIdent[i]='\0';
}

