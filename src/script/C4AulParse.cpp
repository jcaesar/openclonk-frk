/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2001-2004, 2006-2008, 2010  Sven Eberhardt
 * Copyright (c) 2001-2004, 2006-2008, 2010  Peter Wortmann
 * Copyright (c) 2004, 2007  Matthes Bender
 * Copyright (c) 2006-2011  Günther Brammer
 * Copyright (c) 2006, 2010  Armin Burgmeier
 * Copyright (c) 2009-2010  Nicolas Hake
 * Copyright (c) 2010  Benjamin Herr
 * Copyright (c) 2010  Martin Plicht
 * Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de
 *
 * Portions might be copyrighted by other authors who have contributed
 * to OpenClonk.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * See isc_license.txt for full license and disclaimer.
 *
 * "Clonk" is a registered trademark of Matthes Bender.
 * See clonk_trademark_license.txt for full license.
 */
// parses scripts

#include <C4Include.h>
#include <utility>

#include <C4Aul.h>

#include <C4AulDebug.h>
#include <C4Def.h>
#include <C4Game.h>
#include <C4Log.h>
#include <C4Config.h>

#define DEBUG_BYTECODE_DUMP 0

#define C4AUL_Include       "#include"
#define C4AUL_Append        "#appendto"

#define C4AUL_Func          "func"

#define C4AUL_Private       "private"
#define C4AUL_Protected     "protected"
#define C4AUL_Public        "public"
#define C4AUL_Global        "global"
#define C4AUL_Const         "const"

#define C4AUL_If            "if"
#define C4AUL_Else          "else"
#define C4AUL_Do            "do"
#define C4AUL_While         "while"
#define C4AUL_For           "for"
#define C4AUL_In            "in"
#define C4AUL_Return        "return"
#define C4AUL_Var           "Var"
#define C4AUL_Par           "Par"
#define C4AUL_Goto          "goto"
#define C4AUL_Break         "break"
#define C4AUL_Continue      "continue"
#define C4AUL_Inherited     "inherited"
#define C4AUL_SafeInherited "_inherited"
#define C4AUL_this          "this"

#define C4AUL_Image         "Image"
#define C4AUL_Contents      "Contents"
#define C4AUL_Condition     "Condition"
#define C4AUL_Desc          "Desc"

#define C4AUL_MethodAll                 "All"
#define C4AUL_MethodNone                "None"
#define C4AUL_MethodClassic             "Classic"
#define C4AUL_MethodJumpAndRun          "JumpAndRun"

#define C4AUL_GlobalNamed   "static"
#define C4AUL_LocalNamed    "local"
#define C4AUL_VarNamed      "var"

#define C4AUL_TypeInt       "int"
#define C4AUL_TypeBool      "bool"
#define C4AUL_TypeC4ID      "id"
#define C4AUL_TypeDef       "def"
#define C4AUL_TypeEffect    "effect"
#define C4AUL_TypeC4Object  "object"
#define C4AUL_TypePropList  "proplist"
#define C4AUL_TypeString    "string"
#define C4AUL_TypeArray     "array"
#define C4AUL_TypeFunction  "func"

#define C4AUL_True          "true"
#define C4AUL_False         "false"
#define C4AUL_Nil           "nil"
#define C4AUL_New           "new"

#define C4AUL_CodeBufSize   16

// script token type
enum C4AulTokenType
{
	ATT_INVALID,// invalid token
	ATT_DIR,    // directive
	ATT_IDTF,   // identifier
	ATT_INT,    // integer constant
	ATT_STRING, // string constant
	ATT_DOT,    // "."
	ATT_COMMA,  // ","
	ATT_COLON,  // ":"
	ATT_SCOLON, // ";"
	ATT_BOPEN,  // "("
	ATT_BCLOSE, // ")"
	ATT_BOPEN2, // "["
	ATT_BCLOSE2,// "]"
	ATT_BLOPEN, // "{"
	ATT_BLCLOSE,// "}"
	ATT_CALL,   // "->"
	ATT_CALLFS, // "->~"
	ATT_STAR,   // "*"
	ATT_LDOTS,  // '...'
	ATT_SET,    // '='
	ATT_OPERATOR,// operator
	ATT_EOF     // end of file
};

class C4AulParse
{
public:
	enum Type { PARSER, PREPARSER };
	C4AulParse(C4ScriptHost * a, enum Type Type):
			Fn(0), a(a), pOrgScript(a), SPos(a->Script.getData()), TokenSPos(SPos),
			TokenType(ATT_INVALID),
			Done(false),
			Type(Type),
			ContextToExecIn(NULL),
			fJump(false),
			iStack(0),
			pLoopStack(NULL)
	{ }
	~C4AulParse()
	{ while (pLoopStack) PopLoop(); ClearToken(); }
	C4AulScriptFunc *Fn; C4ScriptHost * a; C4ScriptHost * pOrgScript;
	const char *SPos; // current position in the script
	const char *TokenSPos; // start of the current token in the script
	char Idtf[C4AUL_MAX_Identifier]; // current identifier
	C4AulTokenType TokenType; // current token type
	int32_t cInt; // current int constant
	C4String * cStr; // current string constant
	bool Done; // done parsing?
	enum Type Type; // emitting bytecode?
	C4AulScriptContext* ContextToExecIn;
	void Parse_Script();
	void Parse_Function();
	void Parse_Statement();
	void Parse_Block();
	int Parse_Params(int iMaxCnt, const char * sWarn, C4AulFunc * pFunc = 0);
	void Parse_Array();
	void Parse_PropList();
	void Parse_DoWhile();
	void Parse_While();
	void Parse_If();
	void Parse_For();
	void Parse_ForEach();
	void Parse_Expression(int iParentPrio = -1);
	void Parse_Expression2(int iParentPrio = -1);
	void Parse_Var();
	void Parse_Local();
	void Parse_Static();
	void Parse_Const();
	C4Value Parse_ConstExpression(C4PropListStatic * parent, C4String * Name);
	C4Value Parse_ConstPropList(const C4PropListStatic * parent, C4String * Name);

	bool AdvanceSpaces(); // skip whitespaces; return whether script ended
	int GetOperator(const char* pScript);
	// Simply discard the string, put it in the Table and delete it with the script or delete it when refcount drops
	enum OperatorPolicy { OperatorsPlease = 0, StarsPlease };
	void ClearToken(); // clear any data held with the current token
	C4AulTokenType GetNextToken(OperatorPolicy Operator = OperatorsPlease); // get next token of SPos

	void Shift(OperatorPolicy Operator = OperatorsPlease);
	void Match(C4AulTokenType TokenType, const char * Message = NULL);
	void UnexpectedToken(const char * Expected) NORETURN;
	static const char * GetTokenName(C4AulTokenType TokenType);

	void Warn(const char *pMsg, const char *pIdtf=0);
	void Error(const char *pMsg, const char *pIdtf=0);

private:

	bool fJump;
	int iStack;

	int GetStackValue(C4AulBCCType eType, intptr_t X = 0);
	int AddBCC(C4AulBCCType eType, intptr_t X = 0);
	void RemoveLastBCC();
	C4V_Type GetLastRetType(C4V_Type to); // for warning purposes

	C4AulBCC MakeSetter(bool fLeaveValue = false); // Prepares to generate a setter for the last value that was generated

	int JumpHere(); // Get position for a later jump to next instruction added
	void SetJumpHere(int iJumpOp); // Use the next inserted instruction as jump target for the given jump operation
	void SetJump(int iJumpOp, int iWhere);
	void AddJump(C4AulBCCType eType, int iWhere);

	// Keep track of loops and break/continue usages
	struct Loop
	{
		struct Control
		{
			bool Break;
			int Pos;
			Control *Next;
		};
		Control *Controls;
		int StackSize;
		Loop *Next;
	};
	Loop *pLoopStack;

	void PushLoop();
	void PopLoop();
	void AddLoopControl(bool fBreak);
};

void C4AulScript::Warn(const char *pMsg, const char *pIdtf)
{
	// display error

	C4AulParseError warning(this, pMsg, pIdtf, true);
	// display it
	warning.show();
	// count warnings
	++::ScriptEngine.warnCnt;
}

void C4AulParse::Warn(const char *pMsg, const char *pIdtf)
{
	// do not show errors for System.ocg scripts that appear to be pure #appendto scripts
	if (Fn && !Fn->Owner->GetPropList() && !Fn->Owner->Appends.empty()) return;
	// script doesn't own function -> skip
	// (exception: global functions)
	//if(pFunc) if(pFunc->pOrgScript != pScript && pScript != (C4AulScript *)&::ScriptEngine) return;
	// display error

	C4AulParseError warning(this, pMsg, pIdtf, true);
	// display it
	warning.show();
	if (pOrgScript != a)
		DebugLogF("  (as #appendto/#include to %s)", a->ScriptName.getData());
	// count warnings
	++::ScriptEngine.warnCnt;
}

void C4AulParse::Error(const char *pMsg, const char *pIdtf)
{
	throw new C4AulParseError(this, pMsg, pIdtf);
}

C4AulParseError::C4AulParseError(C4AulParse * state, const char *pMsg, const char *pIdtf, bool Warn)
		: C4AulError()
{
	// compose error string
	sMessage.Format("%s: %s%s",
	                Warn ? "WARNING" : "ERROR",
	                pMsg,
	                pIdtf ? pIdtf : "");
	if (state->Fn && state->Fn->GetName())
	{
		// Show function name
		sMessage.AppendFormat(" (in %s", state->Fn->GetName());

		// Exact position
		if (state->Fn->pOrgScript && state->TokenSPos)
			sMessage.AppendFormat(", %s:%d:%d)",
			                      state->Fn->pOrgScript->ScriptName.getData(),
			                      SGetLine(state->Fn->pOrgScript->GetScript(), state->TokenSPos),
			                      SLineGetCharacters(state->Fn->pOrgScript->GetScript(), state->TokenSPos));
		else
			sMessage.AppendChar(')');
	}
	else if (state->pOrgScript)
	{
		// Script name
		sMessage.AppendFormat(" (%s:%d:%d)",
		                      state->pOrgScript->ScriptName.getData(),
		                      SGetLine(state->pOrgScript->GetScript(), state->TokenSPos),
		                      SLineGetCharacters(state->pOrgScript->GetScript(), state->TokenSPos));
	}

}

C4AulParseError::C4AulParseError(C4AulScript *pScript, const char *pMsg, const char *pIdtf, bool Warn)
{
	// compose error string
	sMessage.Format("%s: %s%s",
	                Warn ? "WARNING" : "ERROR",
	                pMsg,
	                pIdtf ? pIdtf : "");
	if (pScript)
	{
		// Script name
		sMessage.AppendFormat(" (%s)",
		                      pScript->ScriptName.getData());
	}
}

bool C4AulParse::AdvanceSpaces()
{
	if (!SPos)
		return false;
	while(*SPos)
	{
		if (*SPos == '/')
		{
			// // comment
			if (SPos[1] == '/')
			{
				SPos += 2;
				while (*SPos && *SPos != 13 && *SPos != 10)
					++SPos;
			}
			// /* comment */
			else if (SPos[1] == '*')
			{
				SPos += 2;
				while (*SPos && (*SPos != '*' || SPos[1] != '/'))
					++SPos;
				SPos += 2;
			}
			else
				return true;
		}
		// Skip any "zero width no-break spaces" (also known as Byte Order Marks)
		else if (*SPos == '\xEF' && SPos[1] == '\xBB' && SPos[2] == '\xBF')
			SPos += 3;
		else if ((unsigned)*SPos > 32)
			return true;
		else
			++SPos;
	}
	// end of script reached
	return false;
}

//=========================== C4Script Operator Map ===================================
struct C4ScriptOpDef
{
	unsigned short Priority;
	const char* Identifier;
	C4AulBCCType Code;
	C4AulBCCType ResultModifier; // code to apply to result after it was calculated
	bool Postfix;
	bool Changer; // changes first operand to result, rewrite to "a = a (op) b"
	bool NoSecondStatement; // no second statement expected (++/-- postfix)
	C4V_Type RetType; // type returned. ignored by C4V
	C4V_Type Type1;
	C4V_Type Type2;
};

static C4ScriptOpDef C4ScriptOpMap[] =
{
	// priority                              postfix
	// |  identifier                         |  changer
	// |  |     Bytecode            Result   |  |  no second id
	// |  |     |                   Modifier |  |  |  RetType   ParType1    ParType2
	// prefix
	{ 15, "++", AB_Inc,             AB_ERR,  0, 1, 0, C4V_Int,  C4V_Int,    C4V_Any},
	{ 15, "--", AB_Dec,             AB_ERR,  0, 1, 0, C4V_Int,  C4V_Int,    C4V_Any},
	{ 15, "~",  AB_BitNot,          AB_ERR,  0, 0, 0, C4V_Int,  C4V_Int,    C4V_Any},
	{ 15, "!",  AB_Not,             AB_ERR,  0, 0, 0, C4V_Bool, C4V_Bool,   C4V_Any},
	{ 15, "+",  AB_ERR,             AB_ERR,  0, 0, 0, C4V_Int,  C4V_Int,    C4V_Any},
	{ 15, "-",  AB_Neg,             AB_ERR,  0, 0, 0, C4V_Int,  C4V_Int,    C4V_Any},
	
	// postfix (whithout second statement)
	{ 16, "++", AB_Inc,             AB_Dec,  1, 1, 1, C4V_Int,  C4V_Int,    C4V_Any},
	{ 16, "--", AB_Dec,             AB_Inc,  1, 1, 1, C4V_Int,  C4V_Int,    C4V_Any},
	
	// postfix
	{ 14, "**", AB_Pow,             AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 13, "/",  AB_Div,             AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 13, "*",  AB_Mul,             AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 13, "%",  AB_Mod,             AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 12, "-",  AB_Sub,             AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 12, "+",  AB_Sum,             AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 11, "<<", AB_LeftShift,       AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 11, ">>", AB_RightShift,      AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 10, "<",  AB_LessThan,        AB_ERR,  1, 0, 0, C4V_Bool, C4V_Int,    C4V_Int},
	{ 10, "<=", AB_LessThanEqual,   AB_ERR,  1, 0, 0, C4V_Bool, C4V_Int,    C4V_Int},
	{ 10, ">",  AB_GreaterThan,     AB_ERR,  1, 0, 0, C4V_Bool, C4V_Int,    C4V_Int},
	{ 10, ">=", AB_GreaterThanEqual,AB_ERR,  1, 0, 0, C4V_Bool, C4V_Int,    C4V_Int},
	{ 9, "==",  AB_Equal,           AB_ERR,  1, 0, 0, C4V_Bool, C4V_Any,    C4V_Any},
	{ 9, "!=",  AB_NotEqual,        AB_ERR,  1, 0, 0, C4V_Bool, C4V_Any,    C4V_Any},
	{ 8, "&",   AB_BitAnd,          AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 6, "^",   AB_BitXOr,          AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 6, "|",   AB_BitOr,           AB_ERR,  1, 0, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 5, "&&",  AB_JUMPAND,         AB_ERR,  1, 0, 0, C4V_Bool, C4V_Bool,   C4V_Bool},
	{ 4, "||",  AB_JUMPOR,          AB_ERR,  1, 0, 0, C4V_Bool, C4V_Bool,   C4V_Bool},
	{ 3, "??",  AB_JUMPNNIL,        AB_ERR,  1, 0, 0, C4V_Bool, C4V_Any,    C4V_Any},
	
	// changers
	{ 2, "*=",  AB_Mul,             AB_ERR,  1, 1, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 2, "/=",  AB_Div,             AB_ERR,  1, 1, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 2, "%=",  AB_Mod,             AB_ERR,  1, 1, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 2, "+=",  AB_Sum,             AB_ERR,  1, 1, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 2, "-=",  AB_Sub,             AB_ERR,  1, 1, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 2, "&=",  AB_BitAnd,          AB_ERR,  1, 1, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 2, "|=",  AB_BitOr,           AB_ERR,  1, 1, 0, C4V_Int,  C4V_Int,    C4V_Int},
	{ 2, "^=",  AB_BitXOr,          AB_ERR,  1, 1, 0, C4V_Int,  C4V_Int,    C4V_Int},
 	
	{ 0, NULL,  AB_ERR,             AB_ERR,  0, 0, 0, C4V_Nil,  C4V_Nil,    C4V_Nil}
};

int C4AulParse::GetOperator(const char* pScript)
{
	// return value:
	// >= 0: operator found. could be found in C4ScriptOfDef
	// -1:   isn't an operator

	unsigned int i;

	if (!*pScript) return 0;
	// operators are not alphabetical
	if ((*pScript >= 'a' && *pScript <= 'z') ||
	    (*pScript >= 'A' && *pScript <= 'Z'))
	{
		return -1;
	}

	// it is a two-char-operator?
	for (i=0; C4ScriptOpMap[i].Identifier; i++)
		if (SLen(C4ScriptOpMap[i].Identifier) == 2)
			if (SEqual2(pScript, C4ScriptOpMap[i].Identifier))
				return i;

	// if not, a one-char one?
	for (i=0; C4ScriptOpMap[i].Identifier; i++)
		if (SLen(C4ScriptOpMap[i].Identifier) == 1)
			if (SEqual2(pScript, C4ScriptOpMap[i].Identifier))
				return i;

	return -1;
}

void C4AulParse::ClearToken()
{
	// if last token was a string, make sure its ref is deleted
	if (TokenType == ATT_STRING && cStr)
	{
		cStr->DecRef();
		TokenType = ATT_INVALID;
	}
}

C4AulTokenType C4AulParse::GetNextToken(OperatorPolicy Operator)
{
	// clear mem of prev token
	ClearToken();
	// move to start of token
	if (!AdvanceSpaces()) return ATT_EOF;
	// store offset
	TokenSPos = SPos;

	// get char
	char C = *(SPos++);
	// Mostly sorted by frequency, except that tokens that have
	// other tokens as prefixes need to be checked for first.
	if (Inside(C, 'a', 'z') || Inside(C, 'A', 'Z') || C == '_' || C == '#')
	{
		// identifier or directive
		bool dir = C == '#';
		int Len = 1;
		C = *SPos;
		while (Inside(C, '0', '9') || Inside(C, 'a', 'z') || Inside(C, 'A', 'Z') || C == '_')
		{
			++Len;
			C = *(++SPos);
		}

		Len = Min(Len, C4AUL_MAX_Identifier);
		SCopy(TokenSPos, Idtf, Len);
		return dir ? ATT_DIR : ATT_IDTF;
	}
	else if (C == '(') return ATT_BOPEN;  // "("
	else if (C == ')') return ATT_BCLOSE; // ")"
	else if (C == ',') return ATT_COMMA;  // ","
	else if (C == ';') return ATT_SCOLON; // ";"
	else if (Inside(C, '0', '9'))
	{
		// integer
		if (C == '0' && *SPos == 'x')
		{
			// hexadecimal
			cInt = StrToI32(SPos + 1, 16, &SPos);
			return ATT_INT;
		}
		else
		{
			// decimal
			cInt = StrToI32(TokenSPos, 10, &SPos);
			return ATT_INT;
		}
	}
	else if (C == '-' && *SPos == '>' && *(SPos + 1) == '~')
		{ SPos+=2; return ATT_CALLFS;}// "->~"
	else if (C == '-' && *SPos == '>')
		{ ++SPos;  return ATT_CALL; } // "->"
	else if (C == '*' && Operator == StarsPlease)
	                   return ATT_STAR;   // "*"
	else if ((cInt = GetOperator(SPos - 1)) != -1)
	{
		SPos += SLen(C4ScriptOpMap[cInt].Identifier) - 1;
		return ATT_OPERATOR;
	}
	else if (C == '=') return ATT_SET;    // "="
	else if (C == '{') return ATT_BLOPEN; // "{"
	else if (C == '}') return ATT_BLCLOSE;// "}"
	else if (C == '"')
	{
		// string
		std::string strbuf;
		strbuf.reserve(512); // assume most strings to be smaller than this
		// string end
		while (*SPos != '"')
		{
			C = *SPos;
			++SPos;
			if (C == '\\') // escape
				switch (*SPos)
				{
				case '"':  ++SPos; strbuf.push_back('"');  break;
				case '\\': ++SPos; strbuf.push_back('\\'); break;
				case 'n':  ++SPos; strbuf.push_back('\n'); break;
				case 't':  ++SPos; strbuf.push_back('\t'); break;
				case 'x':
				{
					++SPos;
					// hexadecimal escape: \xAD.
					// First char must be a hexdigit
					if (!std::isxdigit(*SPos))
					{
						Warn("\\x used with no following hex digits");
						strbuf.push_back('\\'); strbuf.push_back('x');
					}
					else
					{
						char ch = 0;
						while (std::isxdigit(*SPos))
						{
							ch *= 16;
							if (*SPos >= '0' && *SPos <= '9')
								ch += *SPos - '0';
							else if (*SPos >= 'a' && *SPos <= 'f')
								ch += *SPos - 'a' + 10;
							else if (*SPos >= 'A' && *SPos <= 'F')
								ch += *SPos - 'A' + 10;
							++SPos;
						};
						strbuf.push_back(ch);
					}
					break;
				}
				case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
				{
					// Octal escape: \142
					char ch = 0;
					while (SPos[1] >= '0' && SPos[1] <= '7')
					{
						ch *= 8;
						ch += *++SPos -'0';
					}
					strbuf.push_back(ch);
					break;
				}
				default:
				{
					// just insert "\"
					strbuf.push_back('\\');
					// show warning
					char strEscape[2] = { *(SPos + 1), 0 };
					Warn("unknown escape: ", strEscape);
				}
				}
			else if (C == 0 || C == 10 || C == 13) // line break / feed
				throw new C4AulParseError(this, "string not closed");
			else
				// copy character
				strbuf.push_back(C);
		}
		++SPos;
		cStr = Strings.RegString(StdStrBuf(strbuf.data(),strbuf.size()));
		// hold onto string, ClearToken will deref it
		cStr->IncRef();
		return ATT_STRING;
	}
	else if (C == '[') return ATT_BOPEN2; // "["
	else if (C == ']') return ATT_BCLOSE2;// "]"
	else if (C == '.' && *SPos == '.' && *(SPos + 1) == '.')
		{ SPos+=2; return ATT_LDOTS; } // "..."
	else if (C == '.') return ATT_DOT;    // "."
	else if (C == ':') return ATT_COLON;  // ":"
	else
	{
		// show appropriate error message
		if (C >= '!' && C <= '~')
			throw new C4AulParseError(this, FormatString("unexpected character '%c' found", C).getData());
		else
			throw new C4AulParseError(this, FormatString("unexpected character 0x%x found", (int)(unsigned char) C).getData());
	}
}

static const char * GetTTName(C4AulBCCType e)
{
	switch (e)
	{
	case AB_ARRAYA: return "ARRAYA";  // array access
	case AB_ARRAYA_SET: return "ARRAYA_SET";  // setter
	case AB_PROP: return "PROP";
	case AB_PROP_SET: return "PROP_SET";
	case AB_ARRAY_SLICE: return "ARRAY_SLICE";
	case AB_ARRAY_SLICE_SET: return "ARRAY_SLICE_SET";
	case AB_STACK_SET: return "STACK_SET";
	case AB_LOCALN: return "LOCALN";  // a named local
	case AB_LOCALN_SET: return "LOCALN_SET";
	case AB_GLOBALN: return "GLOBALN";  // a named global
	case AB_GLOBALN_SET: return "GLOBALN_SET";
	case AB_PAR: return "PAR";      // Par statement
	case AB_FUNC: return "FUNC";    // function

	case AB_PARN_CONTEXT: return "AB_PARN_CONTEXT";
	case AB_VARN_CONTEXT: return "AB_VARN_CONTEXT";

// prefix
	case AB_Inc: return "Inc";  // ++
	case AB_Dec: return "Dec";  // --
	case AB_BitNot: return "BitNot";  // ~
	case AB_Not: return "Not";  // !
	case AB_Neg: return "Neg";  // -

// postfix
	case AB_Pow: return "Pow";  // **
	case AB_Div: return "Div";  // /
	case AB_Mul: return "Mul";  // *
	case AB_Mod: return "Mod";  // %
	case AB_Sub: return "Sub";  // -
	case AB_Sum: return "Sum";  // +
	case AB_LeftShift: return "LeftShift";  // <<
	case AB_RightShift: return "RightShift";  // >>
	case AB_LessThan: return "LessThan";  // <
	case AB_LessThanEqual: return "LessThanEqual";  // <=
	case AB_GreaterThan: return "GreaterThan";  // >
	case AB_GreaterThanEqual: return "GreaterThanEqual";  // >=
	case AB_Equal: return "Equal";  // ==
	case AB_NotEqual: return "NotEqual";  // !=
	case AB_BitAnd: return "BitAnd";  // &
	case AB_BitXOr: return "BitXOr";  // ^
	case AB_BitOr: return "BitOr";  // |

	case AB_CALL: return "CALL";    // direct object call
	case AB_CALLFS: return "CALLFS";  // failsafe direct call
	case AB_STACK: return "STACK";    // push nulls / pop
	case AB_INT: return "INT";      // constant: int
	case AB_BOOL: return "BOOL";    // constant: bool
	case AB_STRING: return "STRING";  // constant: string
	case AB_CPROPLIST: return "CPROPLIST"; // constant: proplist
	case AB_CARRAY: return "CARRAY";  // constant: array
	case AB_CFUNCTION: return "CFUNCTION";  // constant: function
	case AB_NIL: return "NIL";    // constant: nil
	case AB_NEW_ARRAY: return "NEW_ARRAY";    // semi-constant: array
	case AB_DUP: return "DUP";    // duplicate value from stack
	case AB_NEW_PROPLIST: return "NEW_PROPLIST";    // create a new proplist
	case AB_POP_TO: return "POP_TO";    // initialization of named var
	case AB_JUMP: return "JUMP";    // jump
	case AB_JUMPAND: return "JUMPAND";
	case AB_JUMPOR: return "JUMPOR";
	case AB_JUMPNNIL: return "JUMPNNIL"; // nil-coalescing operator ("??")
	case AB_CONDN: return "CONDN";    // conditional jump (negated, pops stack)
	case AB_COND: return "COND";    // conditional jump (pops stack)
	case AB_FOREACH_NEXT: return "FOREACH_NEXT"; // foreach: next element
	case AB_RETURN: return "RETURN";  // return statement
	case AB_ERR: return "ERR";      // parse error at this position
	case AB_DEBUG: return "DEBUG";      // debug break
	case AB_EOFN: return "EOFN";    // end of function
	case AB_EOF: return "EOF";

	default: return "?";
	}
}

void C4ScriptHost::AddBCC(C4AulBCCType eType, intptr_t X, const char * SPos)
{
	// store chunk
	C4AulBCC bcc;
	bcc.bccType = eType;
	bcc.Par.X = X;
	Code.push_back(bcc);
	PosForCode.push_back(SPos);
	LastCode = &Code.back();

	switch (eType)
	{
	case AB_STRING: case AB_CALL: case AB_CALLFS: case AB_LOCALN: case AB_PROP:
	/* case AB_LOCALN_SET/AB_PROP_SET: -- expected to already have a reference upon creation, see MakeSetter */
		bcc.Par.s->IncRef();
		break;
	default: break;
	}
}

void C4ScriptHost::RemoveLastBCC()
{
	C4AulBCC *pBCC = &Code.back();
	switch (pBCC->bccType)
	{
	case AB_STRING: case AB_CALL: case AB_CALLFS: case AB_LOCALN: case AB_LOCALN_SET: case AB_PROP: case AB_PROP_SET:
		pBCC->Par.s->DecRef();
		break;
	default: break;
	}
	Code.pop_back();
	PosForCode.pop_back();
	if (Code.size())
		LastCode = &Code.back();
	else
		LastCode = NULL;
}

void C4ScriptHost::ClearCode()
{
	while(Code.size() > 0)
		RemoveLastBCC();
	// add one empty chunk to init CPos and for functions without code.
	// For example, leftovers from a previous version of a reloaded script
	AddBCC(AB_ERR);
	C4AulFunc * f = Func0;
	while (f)
	{
		f->SFunc()->CodePos = 0;
		f = f->Next;
	}
}

int C4AulScriptFunc::GetLineOfCode(C4AulBCC * bcc)
{
	return SGetLine(pOrgScript->GetScript(), GetCodeOwner()->PosForCode[bcc - &GetCodeOwner()->Code[0]]);
}

C4AulBCC * C4AulScriptFunc::GetCode()
{
	return &GetCodeOwner()->Code[CodePos];
}

C4ScriptHost * C4AulScriptFunc::GetCodeOwner()
{
	if (Owner == Owner->Engine)
	{
		return pOrgScript->GetScriptHost();
	}
	else
	{
		return Owner->GetScriptHost();
	}
}

bool C4ScriptHost::Preparse()
{
	// handle easiest case first
	if (State < ASS_NONE) return false;

	// clear stuff
	Includes.clear(); Appends.clear();

	if (GetPropList())
		{
		GetPropList()->C4PropList::Clear();
		GetPropList()->SetProperty(P_Prototype, C4VPropList(Engine->GetPropList()));
		}

	// reset code
	ClearCode();

	C4AulParse state(this, C4AulParse::PREPARSER);
	state.Parse_Script();

	// #include will have to be resolved now...
	IncludesResolved = false;

	// return success
	C4AulScript::State = ASS_PREPARSED;
	return true;
}


int C4AulParse::GetStackValue(C4AulBCCType eType, intptr_t X)
{
	switch (eType)
	{
	case AB_INT:
	case AB_BOOL:
	case AB_STRING:
	case AB_CPROPLIST:
	case AB_CARRAY:
	case AB_CFUNCTION:
	case AB_NIL:
	case AB_PARN_CONTEXT:
	case AB_VARN_CONTEXT:
	case AB_LOCALN:
	case AB_GLOBALN:
	case AB_DUP:
		return 1;

	case AB_Pow:
	case AB_Div:
	case AB_Mul:
	case AB_Mod:
	case AB_Sub:
	case AB_Sum:
	case AB_LeftShift:
	case AB_RightShift:
	case AB_LessThan:
	case AB_LessThanEqual:
	case AB_GreaterThan:
	case AB_GreaterThanEqual:
	case AB_Equal:
	case AB_NotEqual:
	case AB_BitAnd:
	case AB_BitXOr:
	case AB_BitOr:
	case AB_PROP_SET:
	case AB_ARRAYA:
	case AB_CONDN:
	case AB_COND:
	case AB_POP_TO:
	case AB_RETURN:
		// JUMPAND/JUMPOR/JUMPNNIL are special: They either jump over instructions adding one to the stack
		// or decrement the stack. Thus, for stack counting purposes, they decrement.
	case AB_JUMPAND:
	case AB_JUMPOR:
	case AB_JUMPNNIL:
		return -1;

	case AB_FUNC:
		return -reinterpret_cast<C4AulFunc *>(X)->GetParCount() + 1;

	case AB_CALL:
	case AB_CALLFS:
		return -C4AUL_MAX_Par;

	case AB_STACK_SET:
	case AB_LOCALN_SET:
	case AB_PROP:
	case AB_GLOBALN_SET:
	case AB_Inc:
	case AB_Dec:
	case AB_BitNot:
	case AB_Not:
	case AB_Neg:
	case AB_PAR:
	case AB_FOREACH_NEXT:
	case AB_ERR:
	case AB_EOFN:
	case AB_EOF:
	case AB_JUMP:
	case AB_DEBUG:
		return 0;

	case AB_STACK:
		return X;

	case AB_NEW_ARRAY:
		return -X+1;

	case AB_NEW_PROPLIST:
		return -X * 2 + 1;

	case AB_ARRAYA_SET:
	case AB_ARRAY_SLICE:
		return -2;

	case AB_ARRAY_SLICE_SET:
		return -3;

	default:
		assert(false);
	}
	return 0;
}

int C4AulParse::AddBCC(C4AulBCCType eType, intptr_t X)
{
	if (Type != PARSER) return -1;

	// Track stack size
	iStack += GetStackValue(eType, X);

	// Use stack operation instead of 0-Any (enable optimization)
	if (eType == AB_NIL)
	{
		eType = AB_STACK;
		X = 1;
	}

	// Join checks only if it's not a jump target
	if (!fJump && a->GetLastCode())
	{
		C4AulBCC *pCPos1 = a->GetLastCode();

		// Skip noop stack operation
		if(eType == AB_STACK && X == 0)
		{
			return a->GetCodePos() - 1;
		}

		// Join together stack operations
		if(eType == AB_STACK && pCPos1->bccType == AB_STACK &&
			(X <= 0 || pCPos1->Par.i >= 0))
		{
			pCPos1->Par.i += X;
			// Empty? Remove it. This relies on the parser not issuing
			// multiple negative stack operations consecutively, as
			// that could result in removing a jump target bytecode.
			if (!pCPos1->Par.i)
				a->RemoveLastBCC();
			return a->GetCodePos() - 1;
		}

		// Prune unneeded Incs / Decs
		if(eType == AB_STACK && X < 0 && (pCPos1->bccType == AB_Inc || pCPos1->bccType == AB_Dec))
		{
			if(C4ScriptOpMap[pCPos1->Par.i].ResultModifier != pCPos1->bccType)
			{
				pCPos1->bccType = eType;
				pCPos1->Par.i = X;
				return a->GetCodePos() - 1;
			}
			else
			{
				// If it was a result modifier, we can safely remove it knowing that it was neither
				// the first chunk nor a jump target. We can therefore apply additional optimizations.
				a->RemoveLastBCC();
				pCPos1--;
			}
		}

		// Join STACK_SET + STACK -1 to POP_TO (equivalent)
		if(eType == AB_STACK && X == -1 && pCPos1->bccType == AB_STACK_SET)
		{
			pCPos1->bccType = AB_POP_TO;
			return a->GetCodePos() - 1;
		}

		// Reduce some constructs like SUM + INT 1 to INC or DEC
		if((eType == AB_Sum || eType == AB_Sub) &&
			pCPos1->bccType == AB_INT &&
			(pCPos1->Par.i == 1 || pCPos1->Par.i == -1))
		{
			if((pCPos1->Par.i > 0) == (eType == AB_Sum))
				pCPos1->bccType = AB_Inc;
			else
				pCPos1->bccType = AB_Dec;
			pCPos1->Par.i = X;
			return a->GetCodePos() - 1;
		}

		// Reduce Not + CONDN to COND
		if(eType == AB_CONDN && pCPos1->bccType == AB_Not)
		{
			pCPos1->bccType = AB_COND;
			pCPos1->Par.i = X;
			return a->GetCodePos() - 1;
		}

	}

	// Add
	a->AddBCC(eType, X, TokenSPos);

	// Reset jump flag
	fJump = false;

	return a->GetCodePos() - 1;
}

void C4AulParse::RemoveLastBCC()
{
	// Security: This is unsafe on anything that might get optimized away
	C4AulBCC *pBCC = a->GetLastCode();
	assert(pBCC->bccType != AB_STACK);
	// Correct stack
	iStack -= GetStackValue(pBCC->bccType, pBCC->Par.X);
	// Remove
	a->RemoveLastBCC();
}

C4V_Type C4AulParse::GetLastRetType(C4V_Type to)
{
	C4V_Type from;
	switch (a->GetLastCode()->bccType)
	{
	case AB_INT: from = Config.Developer.ExtraWarnings || a->GetLastCode()->Par.i ? C4V_Int : C4V_Any; break;
	case AB_STRING: from = C4V_String; break;
	case AB_NEW_ARRAY: case AB_CARRAY: case AB_ARRAY_SLICE: from = C4V_Array; break;
	case AB_CFUNCTION: from = C4V_Function; break;
	case AB_NEW_PROPLIST: case AB_CPROPLIST: from = C4V_PropList; break;
	case AB_BOOL: from = C4V_Bool; break;
	case AB_FUNC:
		from = a->GetLastCode()->Par.f->GetRetType(); break;
	case AB_CALL: case AB_CALLFS:
	{
		C4String * pName = a->GetLastCode()->Par.s;
		C4AulFunc * pFunc2 = a->Engine->GetFirstFunc(pName->GetCStr());
		bool allwarn = true;
		from = C4V_Any;
		while (pFunc2 && allwarn)
		{
			from = pFunc2->GetRetType();
			if (!C4Value::WarnAboutConversion(from, to))
			{
				allwarn = false;
				from = C4V_Any;
			}
			pFunc2 = a->Engine->GetNextSNFunc(pFunc2);
		}
		break;
	}
	case AB_Inc: case AB_Dec: case AB_BitNot: case AB_Neg:
	case AB_Pow: case AB_Div: case AB_Mul: case AB_Mod: case AB_Sub: case AB_Sum:
	case AB_LeftShift: case AB_RightShift: case AB_BitAnd: case AB_BitXOr: case AB_BitOr:
		from = C4V_Int; break;
	case AB_Not: case AB_LessThan: case AB_LessThanEqual: case AB_GreaterThan: case AB_GreaterThanEqual: case AB_Equal: case AB_NotEqual:
		from = C4V_Bool; break;
	default:
		from = C4V_Any; break;
	}
	return from;
}

C4AulBCC C4AulParse::MakeSetter(bool fLeaveValue)
{
	if(Type != PARSER) { C4AulBCC Dummy; Dummy.bccType = AB_ERR; return Dummy; }
	C4AulBCC Value = *(a->GetLastCode()), Setter = Value;
	// Check type
	switch (Value.bccType)
	{
	case AB_ARRAYA: Setter.bccType = AB_ARRAYA_SET; break;
	case AB_ARRAY_SLICE: Setter.bccType = AB_ARRAY_SLICE_SET; break;
	case AB_DUP:
		Setter.bccType = AB_STACK_SET;
		// the setter additionally has the new value on the stack
		--Setter.Par.i;
		break;
	case AB_LOCALN:
		Setter.bccType = AB_LOCALN_SET;
		Setter.Par.s->IncRef(); // so string isn't dropped by RemoveLastBCC, see also C4AulScript::AddBCC
		break;
	case AB_PROP:
		Setter.bccType = AB_PROP_SET;
		Setter.Par.s->IncRef(); // so string isn't dropped by RemoveLastBCC, see also C4AulScript::AddBCC
		break;
	case AB_GLOBALN: Setter.bccType = AB_GLOBALN_SET; break;
	default: 
		throw new C4AulParseError(this, "assignment to a constant");
	}
	// Remove value BCC
	RemoveLastBCC();
	// Want the value?
	if(fLeaveValue)
	{
		// Duplicate parameters on stack
		// (all push one value on the stack as result, so we have -(N-1) parameters)
		int iParCount = -GetStackValue(Value.bccType, Value.Par.X) + 1;
		for(int i = 0; i < iParCount; i++)
			AddBCC(AB_DUP, 1 - iParCount);
		// Finally re-add original BCC
		AddBCC(Value.bccType, Value.Par.X);
	}
	// Done. The returned BCC should be added later once the value to be set was pushed on top.
	assert(GetStackValue(Value.bccType, Value.Par.X) == GetStackValue(Setter.bccType, Setter.Par.X)+1);
	return Setter;
}

int C4AulParse::JumpHere()
{
	// Set flag so the next generated code chunk won't get joined
	fJump = true;
	return a->GetCodePos();
}

static bool IsJump(C4AulBCCType t)
{
	return t == AB_JUMP || t == AB_JUMPAND || t == AB_JUMPOR || t == AB_JUMPNNIL || t == AB_CONDN || t == AB_COND;
}

void C4AulParse::SetJumpHere(int iJumpOp)
{
	if (Type != PARSER) return;
	// Set target
	C4AulBCC *pBCC = a->GetCodeByPos(iJumpOp);
	assert(IsJump(pBCC->bccType));
	pBCC->Par.i = a->GetCodePos() - iJumpOp;
	// Set flag so the next generated code chunk won't get joined
	fJump = true;
}

void C4AulParse::SetJump(int iJumpOp, int iWhere)
{
	if (Type != PARSER) return;
	// Set target
	C4AulBCC *pBCC = a->GetCodeByPos(iJumpOp);
	assert(IsJump(pBCC->bccType));
	pBCC->Par.i = iWhere - iJumpOp;
}

void C4AulParse::AddJump(C4AulBCCType eType, int iWhere)
{
	AddBCC(eType, iWhere - a->GetCodePos());
}

void C4AulParse::PushLoop()
{
	if (Type != PARSER) return;
	Loop *pNew = new Loop();
	pNew->StackSize = iStack;
	pNew->Controls = NULL;
	pNew->Next = pLoopStack;
	pLoopStack = pNew;
}

void C4AulParse::PopLoop()
{
	if (Type != PARSER) return;
	// Delete loop controls
	Loop *pLoop = pLoopStack;
	while (pLoop->Controls)
	{
		// Unlink
		Loop::Control *pCtrl = pLoop->Controls;
		pLoop->Controls = pCtrl->Next;
		// Delete
		delete pCtrl;
	}
	// Unlink & delete
	pLoopStack = pLoop->Next;
	delete pLoop;
}

void C4AulParse::AddLoopControl(bool fBreak)
{
	if (Type != PARSER) return;
	Loop::Control *pNew = new Loop::Control();
	pNew->Break = fBreak;
	pNew->Pos = a->GetCodePos();
	pNew->Next = pLoopStack->Controls;
	pLoopStack->Controls = pNew;
}

const char * C4AulParse::GetTokenName(C4AulTokenType TokenType)
{
	switch (TokenType)
	{
	case ATT_INVALID: return "invalid token";
	case ATT_DIR: return "directive";
	case ATT_IDTF: return "identifier";
	case ATT_INT: return "integer constant";
	case ATT_STRING: return "string constant";
	case ATT_DOT: return "'.'";
	case ATT_COMMA: return "','";
	case ATT_COLON: return "':'";
	case ATT_SCOLON: return "';'";
	case ATT_BOPEN: return "'('";
	case ATT_BCLOSE: return "')'";
	case ATT_BOPEN2: return "'['";
	case ATT_BCLOSE2: return "']'";
	case ATT_BLOPEN: return "'{'";
	case ATT_BLCLOSE: return "'}'";
	case ATT_CALL: return "'->'";
	case ATT_CALLFS: return "'->~'";
	case ATT_STAR: return "'*'";
	case ATT_LDOTS: return "'...'";
	case ATT_SET: return "'='";
	case ATT_OPERATOR: return "operator";
	case ATT_EOF: return "end of file";
	default: return "unrecognized token";
	}
}

void C4AulParse::Shift(OperatorPolicy Operator)
{
	TokenType = GetNextToken(Operator);
}
void C4AulParse::Match(C4AulTokenType RefTokenType, const char * Message)
{
	if (TokenType != RefTokenType)
		// error
		throw new C4AulParseError(this, Message ? Message :
		                          FormatString("%s expected, but found %s", GetTokenName(RefTokenType), GetTokenName(TokenType)).getData());
	Shift();
}
void C4AulParse::UnexpectedToken(const char * Expected)
{
	throw new C4AulParseError(this, FormatString("%s expected, but found %s", Expected, GetTokenName(TokenType)).getData());
}

void C4AulScriptFunc::ParseFn(C4AulScriptContext* context)
{
	// store byte code pos
	// (relative position to code start; code pointer may change while
	//  parsing)
	CodePos = GetCodeOwner()->Code.size();
	// parse
	C4AulParse state(GetCodeOwner(), C4AulParse::PARSER);
	state.ContextToExecIn = context;
	// get first token
	state.SPos = Script;
	state.Shift();
	state.Fn = this;
	state.Parse_Expression();
	state.Match(ATT_EOF);
	GetCodeOwner()->AddBCC(AB_RETURN, 0, state.TokenSPos);
}

void C4AulParse::Parse_Script()
{
	const char * SPos0 = SPos;
	bool all_ok = true;
	bool found_code = false;
	while (true) try
	{
		// Go to the next token if the current token could not be processed or no token has yet been parsed
		if (SPos == SPos0)
		{
			Shift();
		}
		SPos0 = SPos;
		switch (TokenType)
		{
			case ATT_DIR:
			{
				if (found_code)
					Warn(FormatString("Found %s after declarations", Idtf).getData());
				// check for include statement
				if (SEqual(Idtf, C4AUL_Include))
				{
					Shift();
					// get id of script to include
					if (TokenType != ATT_IDTF)
						UnexpectedToken("identifier");
					if (Type == PREPARSER)
					{
						// add to include list
						a->Includes.push_back(C4ID(StdStrBuf(Idtf)));
					}
					Shift();
				}
				else if (SEqual(Idtf, C4AUL_Append))
				{
					if (pOrgScript->GetPropList()->GetDef())
						throw new C4AulParseError(this, "#appendto in a Definition");
					// for #appendto * '*' needs to be ATT_STAR, not an operator.
					Shift(StarsPlease);
					if (Type == PREPARSER)
					{
						// get id of script to include/append
						C4ID Id;
						switch (TokenType)
						{
						case ATT_IDTF:
							Id = C4ID(StdStrBuf(Idtf));
							break;
						case ATT_STAR: // "*"
							Id = C4ID::None;
							break;
						default:
							// -> ID expected
							UnexpectedToken("identifier or '*'");
						}
						// add to append list
						a->Appends.push_back(Id);
					}
					Shift();
				}
				else
					// -> unknown directive
					throw new C4AulParseError(this, "unknown directive: ", Idtf);
				break;
			}
			case ATT_IDTF:
			{
				// need a keyword here to avoid parsing random function contents
				// after a syntax error in a function
				found_code = true;
				// check for object-local variable definition (local)
				if (SEqual(Idtf, C4AUL_LocalNamed))
				{
					Parse_Local();
					Match(ATT_SCOLON);
					break;
				}
				// check for variable definition (static)
				else if (SEqual(Idtf, C4AUL_GlobalNamed))
				{
					Parse_Static();
					Match(ATT_SCOLON);
					break;
				}
				else
					Parse_Function();
				break;
			}
			case ATT_EOF:
				return;
			default:
				UnexpectedToken("declaration");
		}
		all_ok = true;
	}
	catch (C4AulError *err)
	{
		// damn! something went wrong, print it out
		// but only one error per function
		if (all_ok)
		{
			err->show();
			// show a warning if the error is in a remote script
			if (pOrgScript != a)
				DebugLogF("  (as #appendto/#include to %s)", a->ScriptName.getData());
			// and count (visible only ;) )
			++::ScriptEngine.errCnt;
		}
		all_ok = false;
		delete err;

		// do not show errors for System.ocg scripts that appear to be pure #appendto scripts
		//if (Fn->Owner->GetPropList() || Fn->Owner->Appends.empty()) show
		if (Fn)
		{
			// make all jumps that don't have their destination yet jump here
			for (unsigned int i = Fn->CodePos; i < a->Code.size(); i++)
			{
				C4AulBCC *pBCC = &a->Code[i];
				if (IsJump(pBCC->bccType))
					if (!pBCC->Par.i)
						pBCC->Par.i = a->Code.size() - i;
			}
			// add an error chunk
			AddBCC(AB_ERR);
		}
	}
}

void C4AulParse::Parse_Function()
{
	C4AulAccess Acc = AA_PUBLIC;
	// Access?
	if (SEqual(Idtf, C4AUL_Private)) { Acc = AA_PRIVATE; Shift(); }
	else if (SEqual(Idtf, C4AUL_Protected)) { Acc = AA_PROTECTED; Shift(); }
	else if (SEqual(Idtf, C4AUL_Public)) { Acc = AA_PUBLIC; Shift(); }
	else if (SEqual(Idtf, C4AUL_Global)) { Acc = AA_GLOBAL; Shift(); }
	// check for func declaration
	if (!SEqual(Idtf, C4AUL_Func))
		throw new C4AulParseError(this, "Declaration expected, but found identifier ", Idtf);
	Shift();
	// get next token, must be func name
	if (TokenType != ATT_IDTF)
		UnexpectedToken("function name");
	// check: symbol already in use?
	switch (Acc)
	{
	case AA_PRIVATE:
	case AA_PROTECTED:
	case AA_PUBLIC:
		if (a->LocalNamed.GetItemNr(Idtf) != -1)
			throw new C4AulParseError(this, "function definition: name already in use (local variable)");
		if (a->GetPropList())
			break;
		// func in global context: fallthru
	case AA_GLOBAL:
		if (a != pOrgScript)
			throw new C4AulParseError(this, "global func in appendto/included script: ", Idtf);
		if (a->Engine->GlobalNamedNames.GetItemNr(Idtf) != -1)
			throw new C4AulParseError(this, "function definition: name already in use (global variable)");
		if (a->Engine->GlobalConstNames.GetItemNr(Idtf) != -1)
			Error("function definition: name already in use (global constant)", 0);
	}
	// get script fn
	C4AulScript * owner;
	if (Acc == AA_GLOBAL)
		owner = a->Engine;
	else
		owner = a;
	Fn = 0;
	C4AulFunc * f = owner->Func0;
	while (f)
	{
		if (SEqual(f->GetName(), Idtf) && f->SFunc() && f->SFunc()->pOrgScript == pOrgScript)
		{
			if (Fn)
				//throw new C4AulParseError(this, "Duplicate function ", Idtf);
				Warn("Duplicate function ", Idtf);
			Fn = f->SFunc();
		}
		f = f->Next;
	}
	// first preparser run or a new func in a reloaded script
	if (!Fn && Type == PREPARSER)
	{
		Fn = new C4AulScriptFunc(owner, pOrgScript, Idtf, SPos);
	}
	assert(Fn);
	if (Type == PARSER)
	{
		assert(Fn->GetCodeOwner() == a);
		Fn->CodePos = a->Code.size();
	}

	// Parse function body
	Shift();
	// expect an opening bracket now
	if (TokenType != ATT_BOPEN)
		UnexpectedToken("'('");
	Shift();
	// get pars
	int cpar = 0;
	while (TokenType != ATT_BCLOSE)
	{
		// too many parameters?
		if (cpar >= C4AUL_MAX_Par)
			throw new C4AulParseError(this, "'func' parameter list: too many parameters (max 10)");
		// must be a name or type now
		if (TokenType != ATT_IDTF)
		{
			UnexpectedToken("parameter or closing bracket");
		}
		// type identifier?
		C4V_Type t = C4V_Any;
		if (SEqual(Idtf, C4AUL_TypeInt)) { t = C4V_Int; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypeBool)) { t = C4V_Bool; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypeC4ID)) { t = C4V_Def; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypeDef)) { t = C4V_Def; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypeEffect)) { t = C4V_Effect; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypeC4Object)) { t = C4V_Object; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypePropList)) { t = C4V_PropList; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypeString)) { t = C4V_String; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypeArray)) { t = C4V_Array; Shift(); }
		else if (SEqual(Idtf, C4AUL_TypeFunction)) { t = C4V_Function; Shift(); }
		Fn->ParType[cpar] = t;
		if (TokenType == ATT_BCLOSE || TokenType == ATT_COMMA)
		{
			if (Type == PREPARSER) Fn->AddPar(Idtf);
			if (Config.Developer.ExtraWarnings)
				Warn(FormatString("'%s' used as parameter name", Idtf).getData());
		}
		else if (TokenType != ATT_IDTF)
		{
			UnexpectedToken("parameter name");
		}
		else
		{
			if (Type == PREPARSER) Fn->AddPar(Idtf);
			Shift();
		}
		// end of params?
		if (TokenType == ATT_BCLOSE)
		{
			break;
		}
		// must be a comma now
		if (TokenType != ATT_COMMA)
			UnexpectedToken("comma or closing bracket");
		Shift();
		cpar++;
	}
	Match(ATT_BCLOSE);
	Match(ATT_BLOPEN);
	// Push variables
	if (Fn->VarNamed.iSize)
		AddBCC(AB_STACK, Fn->VarNamed.iSize);
	iStack = 0;
	while (TokenType != ATT_BLCLOSE)
	{
		Parse_Statement();
		assert(!iStack);
	}
	// return nil if the function doesn't return anything
	C4AulBCC * CPos = a->GetLastCode();
	if (!CPos || CPos->bccType != AB_RETURN || fJump)
	{
		if (C4AulDebug::GetDebugger())
			AddBCC(AB_DEBUG);
		AddBCC(AB_NIL);
		AddBCC(AB_RETURN);
	}
	// add separator
	AddBCC(AB_EOFN);
	// Do not blame this function for script errors between functions
	Fn = 0;
	Shift();
}

void C4AulParse::Parse_Block()
{
	Match(ATT_BLOPEN);
	while (TokenType != ATT_BLCLOSE)
	{
		Parse_Statement();
	}
	Shift();
}

void C4AulParse::Parse_Statement()
{
	if (C4AulDebug::GetDebugger())
		AddBCC(AB_DEBUG);
	switch (TokenType)
	{
		// do we have a block start?
	case ATT_BLOPEN:
	{
		Parse_Block();
		return;
	}
	case ATT_BOPEN:
	case ATT_BOPEN2:
	case ATT_SET:
	case ATT_OPERATOR:
	case ATT_INT:
	case ATT_STRING:
	{
		Parse_Expression();
		AddBCC(AB_STACK, -1);
		Match(ATT_SCOLON);
		return;
	}
	// additional function separator
	case ATT_SCOLON:
	{
		Shift();
		break;
	}
	case ATT_IDTF:
	{
		// check for variable definition (var)
		if (SEqual(Idtf, C4AUL_VarNamed))
			Parse_Var();
		// check for variable definition (local)
		else if (SEqual(Idtf, C4AUL_LocalNamed))
			Parse_Local();
		// check for variable definition (static)
		else if (SEqual(Idtf, C4AUL_GlobalNamed))
			Parse_Static();
		// check new-form func begin
		else if (SEqual(Idtf, C4AUL_Func) ||
		         SEqual(Idtf, C4AUL_Private) ||
		         SEqual(Idtf, C4AUL_Protected) ||
		         SEqual(Idtf, C4AUL_Public) ||
		         SEqual(Idtf, C4AUL_Global))
		{
			throw new C4AulParseError(this, "unexpected end of function");
		}
		// get function by identifier: first check special functions
		else if (SEqual(Idtf, C4AUL_If)) // if
		{
			Parse_If();
			break;
		}
		else if (SEqual(Idtf, C4AUL_Else)) // else
		{
			throw new C4AulParseError(this, "misplaced 'else'");
		}
		else if (SEqual(Idtf, C4AUL_Do)) // while
		{
			Parse_DoWhile();
			break;
		}
		else if (SEqual(Idtf, C4AUL_While)) // while
		{
			Parse_While();
			break;
		}
		else if (SEqual(Idtf, C4AUL_For)) // for
		{
			Shift();
			// Look if it's the for([var] foo in array)-form
			const char * SPos0 = SPos;
			// must be followed by a bracket
			Match(ATT_BOPEN);
			// optional var
			if (TokenType == ATT_IDTF && SEqual(Idtf, C4AUL_VarNamed))
				Shift();
			// variable and "in"
			if (TokenType == ATT_IDTF /*&& (iVarID = Fn->VarNamed.GetItemNr(Idtf)) != -1*/
			    && GetNextToken() == ATT_IDTF
			    && SEqual(Idtf, C4AUL_In))
			{
				// reparse the stuff in the brackets like normal statements
				SPos = SPos0;
				Shift();
				Parse_ForEach();
			}
			else
			{
				// reparse the stuff in the brackets like normal statements
				SPos = SPos0;
				Shift();
				Parse_For();
			}
			break;
		}
		else if (SEqual(Idtf, C4AUL_Return)) // return
		{
			Shift();
			if (TokenType == ATT_SCOLON)
			{
				// allow return; without return value (implies nil)
				AddBCC(AB_NIL);
			}
			else
			{
				// return retval;
				Parse_Expression();
			}
			AddBCC(AB_RETURN);
		}
		else if (SEqual(Idtf, C4AUL_Break)) // break
		{
			Shift();
			if (Type == PARSER)
			{
				// Must be inside a loop
				if (!pLoopStack)
				{
					Error("'break' is only allowed inside loops");
				}
				else
				{
					// Insert code
					if (pLoopStack->StackSize != iStack)
						AddBCC(AB_STACK, pLoopStack->StackSize - iStack);
					AddLoopControl(true);
					AddBCC(AB_JUMP);
				}
			}
		}
		else if (SEqual(Idtf, C4AUL_Continue)) // continue
		{
			Shift();
			if (Type == PARSER)
			{
				// Must be inside a loop
				if (!pLoopStack)
				{
					Error("'continue' is only allowed inside loops");
				}
				else
				{
					// Insert code
					if (pLoopStack->StackSize != iStack)
						AddBCC(AB_STACK, pLoopStack->StackSize - iStack);
					AddLoopControl(false);
					AddBCC(AB_JUMP);
				}
			}
		}
		else
		{
			Parse_Expression();
			AddBCC(AB_STACK, -1);
		}
		Match(ATT_SCOLON);
		break;
	}
	default:
	{
		// -> unexpected token
		UnexpectedToken("statement");
	}
	}
}

int C4AulParse::Parse_Params(int iMaxCnt, const char * sWarn, C4AulFunc * pFunc)
{
	int size = 0;
	// so it's a regular function; force "("
	Match(ATT_BOPEN);
	bool fDone = false;
	do switch (TokenType)
	{
		case ATT_BCLOSE:
		{
			Shift();
			if (size > 0)
			{
				if (sWarn && Config.Developer.ExtraWarnings)
					Warn(FormatString("parameter %d of call to %s is empty", size, sWarn).getData(), NULL);
			}
			fDone = true;
			break;
		}
		case ATT_COMMA:
		{
			// got no parameter before a ","
			if (sWarn && Config.Developer.ExtraWarnings)
				Warn(FormatString("parameter %d of call to %s is empty", size, sWarn).getData(), NULL);
			AddBCC(AB_NIL);
			Shift();
			++size;
			break;
		}
		case ATT_LDOTS:
		{
			// functions using ... always take as many parameters as possible
			assert(Type == PREPARSER || Fn->ParCount == C4AUL_MAX_Par);
			Fn->ParCount = C4AUL_MAX_Par;
			Shift();
			// Push all unnamed parameters of the current function as parameters
			int i = Fn->ParNamed.iSize;
			while (size < iMaxCnt && i < C4AUL_MAX_Par)
			{
				AddBCC(AB_DUP, 1 + i - (iStack + Fn->VarNamed.iSize + Fn->GetParCount()));
				++i;
				++size;
			}
			// Do not allow more parameters even if there is place left
			fDone = true;
			Match(ATT_BCLOSE);
			break;
		}
		default:
		{
			// get a parameter
			Parse_Expression();
			if (pFunc && (Type == PARSER) && size < iMaxCnt)
			{
				C4V_Type to = pFunc->GetParType()[size];
				// pFunc either is the return value from a GetFirstFunc-Call or
				// the only function that could be called. When in doubt, don't warn.
				C4AulFunc * pFunc2 = pFunc;
				while ((pFunc2 = a->Engine->GetNextSNFunc(pFunc2)))
					if (pFunc2->GetParType()[size] != to) to = C4V_Any;
				C4V_Type from = GetLastRetType(to);
				if (C4Value::WarnAboutConversion(from, to))
				{
					Warn(FormatString("parameter %d of call to %s is %s instead of %s", size, sWarn, GetC4VName(from), GetC4VName(to)).getData(), NULL);
				}
			}
			++size;
			// end of parameter list?
			if (TokenType == ATT_COMMA)
				Shift();
			else if (TokenType == ATT_BCLOSE)
			{
				Shift();
				fDone = true;
			}
			else UnexpectedToken("',' or ')'");
			break;
		}
	}
	while (!fDone);
	// too many parameters?
	if (sWarn && size > iMaxCnt && Type == PARSER)
		Warn(FormatString("call to %s gives %d parameters, but only %d are used", sWarn, size, iMaxCnt).getData(), NULL);
	// Balance stack
	if (size != iMaxCnt)
		AddBCC(AB_STACK, iMaxCnt - size);
	return size;
}

void C4AulParse::Parse_Array()
{
	// force "["
	Match(ATT_BOPEN2);
	// Create an array
	int size = 0;
	bool fDone = false;
	do
		switch (TokenType)
		{
		case ATT_BCLOSE2:
		{
			Shift();
			// [] -> size 0, [*,] -> size 2, [*,*,] -> size 3
			if (size > 0)
			{
				if (Config.Developer.ExtraWarnings)
					Warn(FormatString("array entry %d is empty", size).getData(), NULL);
				AddBCC(AB_NIL);
				++size;
			}
			fDone = true;
			break;
		}
		case ATT_COMMA:
		{
			// got no parameter before a ","? then push nil
			if (Config.Developer.ExtraWarnings)
				Warn(FormatString("array entry %d is empty", size).getData(), NULL);
			AddBCC(AB_NIL);
			Shift();
			++size;
			break;
		}
		default:
		{
			Parse_Expression();
			++size;
			if (TokenType == ATT_COMMA)
				Shift();
			else if (TokenType == ATT_BCLOSE2)
			{
				Shift();
				fDone = true;
				break;
			}
			else
				UnexpectedToken("',' or ']'");
		}
		}
	while (!fDone);
	// add terminator
	AddBCC(AB_NEW_ARRAY, size);
}

void C4AulParse::Parse_PropList()
{
	int size = 0;
	if (TokenType == ATT_IDTF && SEqual(Idtf, C4AUL_New))
	{
		Shift();
		AddBCC(AB_STRING, (intptr_t) &Strings.P[P_Prototype]);
		Parse_Expression();
		C4V_Type from = GetLastRetType(C4V_PropList);
		if (C4Value::WarnAboutConversion(from, C4V_PropList))
		{
			Warn(FormatString("Prototype is %s instead of %s", GetC4VName(from), GetC4VName(C4V_PropList)).getData(), NULL);
		}
		if (a->GetLastCode()->bccType == AB_CPROPLIST && a->GetLastCode()->Par.p->GetDef())
		{
			throw new C4AulParseError(this, "Can't use new on definitions yet.");
		}
		++size;
	}
	Match(ATT_BLOPEN);
	while (TokenType != ATT_BLCLOSE)
	{
		C4String * pKey;
		if (TokenType == ATT_IDTF)
		{
			pKey = Strings.RegString(Idtf);
			AddBCC(AB_STRING, (intptr_t) pKey);
			Shift();
		}
		else if (TokenType == ATT_STRING)
		{
			AddBCC(AB_STRING, reinterpret_cast<intptr_t>(cStr));
			Shift();
		}
		else UnexpectedToken("string or identifier");
		if (TokenType != ATT_COLON && TokenType != ATT_SET)
			UnexpectedToken("':' or '='");
		Shift();
		Parse_Expression();
		++size;
		if (TokenType == ATT_COMMA)
			Shift();
		else if (TokenType != ATT_BLCLOSE)
			UnexpectedToken("'}' or ','");
	}
	AddBCC(AB_NEW_PROPLIST, size);
	Shift();
}

C4Value C4AulParse::Parse_ConstPropList(const C4PropListStatic * parent, C4String * Name)
{
	if (!Name)
		throw new C4AulParseError(this, "a static proplist is not allowed to be anonymous");
	C4PropListStatic * p;
	if (Type == PREPARSER)
	{
		p = C4PropList::NewAnon(NULL, parent, Name);
	}
	else
	{
		C4Value v;
		bool r;
		if (parent)
			r = parent->GetPropertyByS(Name, &v);
		else
			r = a->Engine->GetGlobalConstant(Name->GetCStr(), &v);
		if (!r || !v.getPropList())
		{
			// the proplist couldn't be parsed or was overwritten by a later constant.
			// create a temporary replacement
			v.SetPropList(C4PropList::NewAnon(NULL, parent, Name));
		}
		p = v.getPropList()->IsStatic();
		if (!p)
			throw new C4AulParseError(this, "internal error: constant proplist is not static");
		if (p->GetParent() != parent || p->GetParentKeyName() != Name)
		{
			// the proplist was overwritten by one in another script
			// create a temporary replacement
			v.SetPropList(C4PropList::NewAnon(NULL, parent, Name));
			p = v.getPropList()->IsStatic();
		}
		// In case of script reloads
		p->Thaw();
	}
	Shift();
	while (TokenType != ATT_BLCLOSE)
	{
		C4String * pKey;
		if (TokenType == ATT_IDTF)
		{
			pKey = Strings.RegString(Idtf);
			Shift();
		}
		else if (TokenType == ATT_STRING)
		{
			pKey = cStr;
			Shift();
		}
		else UnexpectedToken("string or identifier");
		if (TokenType != ATT_COLON && TokenType != ATT_SET)
			UnexpectedToken("':' or '='");
		Shift();
		Parse_ConstExpression(p, pKey);
		if (TokenType == ATT_COMMA)
			Shift();
		else if (TokenType != ATT_BLCLOSE)
			UnexpectedToken("'}' or ','");
	}
	if (Type == PARSER)
		p->Freeze();
	return C4VPropList(p);
}

void C4AulParse::Parse_DoWhile()
{
	Shift();
	// Save position for later jump back
	int iStart = JumpHere();
	// We got a loop
	PushLoop();
	// Execute body
	Parse_Statement();
	// Execute condition
	if (TokenType != ATT_IDTF || !SEqual(Idtf, C4AUL_While))
		UnexpectedToken("'while'");
	Shift();
	Match(ATT_BOPEN);
	Parse_Expression();
	Match(ATT_BCLOSE);
	// Jump back
	AddJump(AB_COND, iStart);
	if (Type != PARSER) return;
	// Set targets for break/continue
	for (Loop::Control *pCtrl = pLoopStack->Controls; pCtrl; pCtrl = pCtrl->Next)
		if (pCtrl->Break)
			SetJumpHere(pCtrl->Pos);
		else
			SetJump(pCtrl->Pos, iStart);
	PopLoop();
}

void C4AulParse::Parse_While()
{
	Shift();
	// Save position for later jump back
	int iStart = JumpHere();
	// Execute condition
	Match(ATT_BOPEN);
	Parse_Expression();
	Match(ATT_BCLOSE);
	// Check condition
	int iCond = AddBCC(AB_CONDN);
	// We got a loop
	PushLoop();
	// Execute body
	Parse_Statement();
	if (Type != PARSER) return;
	// Jump back
	AddJump(AB_JUMP, iStart);
	// Set target for conditional jump
	SetJumpHere(iCond);
	// Set targets for break/continue
	for (Loop::Control *pCtrl = pLoopStack->Controls; pCtrl; pCtrl = pCtrl->Next)
		if (pCtrl->Break)
			SetJumpHere(pCtrl->Pos);
		else
			SetJump(pCtrl->Pos, iStart);
	PopLoop();
}

void C4AulParse::Parse_If()
{
	Shift();
	Match(ATT_BOPEN);
	Parse_Expression();
	Match(ATT_BCLOSE);
	// create bytecode, remember position
	int iCond = AddBCC(AB_CONDN);
	// parse controlled statement
	Parse_Statement();
	if (TokenType == ATT_IDTF && SEqual(Idtf, C4AUL_Else))
	{
		// add jump
		int iJump = AddBCC(AB_JUMP);
		// set condition jump target
		SetJumpHere(iCond);
		Shift();
		// expect a command now
		Parse_Statement();
		// set jump target
		SetJumpHere(iJump);
	}
	else
		// set condition jump target
		SetJumpHere(iCond);
}

void C4AulParse::Parse_For()
{
	// Initialization
	if (TokenType == ATT_IDTF && SEqual(Idtf, C4AUL_VarNamed))
	{
		Parse_Var();
	}
	else if (TokenType != ATT_SCOLON)
	{
		Parse_Expression();
		AddBCC(AB_STACK, -1);
	}
	// Consume first semicolon
	Match(ATT_SCOLON);
	// Condition
	int iCondition = -1, iJumpBody = -1, iJumpOut = -1;
	if (TokenType != ATT_SCOLON)
	{
		// Add condition code
		iCondition = JumpHere();
		Parse_Expression();
		// Jump out
		iJumpOut = AddBCC(AB_CONDN);
	}
	// Consume second semicolon
	Match(ATT_SCOLON);
	// Incrementor
	int iIncrementor = -1;
	if (TokenType != ATT_BCLOSE)
	{
		// Must jump over incrementor
		iJumpBody = AddBCC(AB_JUMP);
		// Add incrementor code
		iIncrementor = JumpHere();
		Parse_Expression();
		AddBCC(AB_STACK, -1);
		// Jump to condition
		if (iCondition != -1)
			AddJump(AB_JUMP, iCondition);
	}
	// Consume closing bracket
	Match(ATT_BCLOSE);
	// Allow break/continue from now on
	PushLoop();
	// Body
	int iBody = JumpHere();
	if (iJumpBody != -1)
		SetJumpHere(iJumpBody);
	Parse_Statement();
	if (Type != PARSER) return;
	// Where to jump back?
	int iJumpBack;
	if (iIncrementor != -1)
		iJumpBack = iIncrementor;
	else if (iCondition != -1)
		iJumpBack = iCondition;
	else
		iJumpBack = iBody;
	AddJump(AB_JUMP, iJumpBack);
	// Set target for condition
	if (iJumpOut != -1)
		SetJumpHere(iJumpOut);
	// Set targets for break/continue
	for (Loop::Control *pCtrl = pLoopStack->Controls; pCtrl; pCtrl = pCtrl->Next)
		if (pCtrl->Break)
			SetJumpHere(pCtrl->Pos);
		else
			SetJump(pCtrl->Pos, iJumpBack);
	PopLoop();
}

void C4AulParse::Parse_ForEach()
{
	if (TokenType == ATT_IDTF && SEqual(Idtf, C4AUL_VarNamed))
	{
		Shift();
	}
	// get variable name
	if (TokenType != ATT_IDTF)
		UnexpectedToken("variable name");
	if (Type == PREPARSER)
	{
		// insert variable
		Fn->VarNamed.AddName(Idtf);
	}
	// search variable (fail if not found)
	int iVarID = Fn->VarNamed.GetItemNr(Idtf);
	if (iVarID < 0)
		throw new C4AulParseError(this, "internal error: var definition: var not found in variable table");
	Shift();
	if (TokenType != ATT_IDTF || !SEqual(Idtf, C4AUL_In))
		UnexpectedToken("'in'");
	Shift();
	// get expression for array
	Parse_Expression();
	Match(ATT_BCLOSE);
	// push initial position (0)
	AddBCC(AB_INT);
	// get array element
	int iStart = AddBCC(AB_FOREACH_NEXT, iVarID);
	// jump out (FOREACH_NEXT will jump over this if
	// we're not at the end of the array yet)
	int iCond = AddBCC(AB_JUMP);
	// got a loop...
	PushLoop();
	// loop body
	Parse_Statement();
	if (Type != PARSER) return;
	// jump back
	AddJump(AB_JUMP, iStart);
	// set condition jump target
	SetJumpHere(iCond);
	// set jump targets for break/continue
	for (Loop::Control *pCtrl = pLoopStack->Controls; pCtrl; pCtrl = pCtrl->Next)
		if (pCtrl->Break)
			SetJumpHere(pCtrl->Pos);
		else
			SetJump(pCtrl->Pos, iStart);
	PopLoop();
	// remove array and counter from stack
	AddBCC(AB_STACK, -2);
}

void C4AulParse::Parse_Expression(int iParentPrio)
{
	switch (TokenType)
	{
	case ATT_IDTF:
	{
		int ndx;
		// check for parameter (par)
		if (Fn->ParNamed.GetItemNr(Idtf) != -1)
		{
			// insert variable by id
			AddBCC(AB_DUP, 1 + Fn->ParNamed.GetItemNr(Idtf) - (iStack + Fn->VarNamed.iSize + Fn->GetParCount()));
			Shift();
		}
		// check for variable (var)
		else if (Fn->VarNamed.GetItemNr(Idtf) != -1)
		{
			// insert variable by id
			AddBCC(AB_DUP, 1 + Fn->VarNamed.GetItemNr(Idtf) - (iStack + Fn->VarNamed.iSize));
			Shift();
		}
		else if (ContextToExecIn && (ndx = ContextToExecIn->Func->ParNamed.GetItemNr(Idtf)) != -1)
		{
			AddBCC(AB_PARN_CONTEXT, ndx);
			Shift();
		}
		else if (ContextToExecIn && (ndx = ContextToExecIn->Func->VarNamed.GetItemNr(Idtf)) != -1)
		{
			AddBCC(AB_VARN_CONTEXT, ndx);
			Shift();
		}
		// check for variable (local)
		else if (a->LocalNamed.GetItemNr(Idtf) != -1)
		{
			// global func?
			if (Fn->Owner == &::ScriptEngine)
				throw new C4AulParseError(this, "using local variable in global function!");
			// insert variable by id
			C4String * pKey = Strings.RegString(Idtf);
			AddBCC(AB_LOCALN, (intptr_t) pKey);
			Shift();
		}
		else if (SEqual(Idtf, C4AUL_True))
		{
			AddBCC(AB_BOOL, 1);
			Shift();
		}
		else if (SEqual(Idtf, C4AUL_False))
		{
			AddBCC(AB_BOOL, 0);
			Shift();
		}
		else if (SEqual(Idtf, C4AUL_Nil))
		{
			AddBCC(AB_NIL);
			Shift();
		}
		else if (SEqual(Idtf, C4AUL_New))
		{
			Parse_PropList();
		}
		// check for global variable (static)
		else if (a->Engine->GlobalNamedNames.GetItemNr(Idtf) != -1)
		{
			// insert variable by id
			AddBCC(AB_GLOBALN, a->Engine->GlobalNamedNames.GetItemNr(Idtf));
			Shift();
		}
		// function identifier: check special functions
		else if (SEqual(Idtf, C4AUL_If))
			// -> if is not a valid parameter
			throw new C4AulParseError(this, "'if' may not be used as a parameter");
		else if (SEqual(Idtf, C4AUL_While))
			// -> while is not a valid parameter
			throw new C4AulParseError(this, "'while' may not be used as a parameter");
		else if (SEqual(Idtf, C4AUL_Else))
			// -> else is not a valid parameter
			throw new C4AulParseError(this, "misplaced 'else'");
		else if (SEqual(Idtf, C4AUL_For))
			// -> for is not a valid parameter
			throw new C4AulParseError(this, "'for' may not be used as a parameter");
		else if (SEqual(Idtf, C4AUL_Return))
		{
			Error("return may not be used as a parameter", 0);
		}
		else if (SEqual(Idtf, C4AUL_Par))
		{
			// functions using Par() always take as many parameters as possible
			Fn->ParCount = C4AUL_MAX_Par;
			// and for Par
			Shift();
			Parse_Params(1, C4AUL_Par);
			AddBCC(AB_PAR);
		}
		else if (SEqual(Idtf, C4AUL_Inherited) || SEqual(Idtf, C4AUL_SafeInherited))
		{
			Shift();
			// get function
			if (Fn->OwnerOverloaded)
			{
				// add direct call to byte code
				Parse_Params(Fn->OwnerOverloaded->GetParCount(), NULL, Fn->OwnerOverloaded);
				AddBCC(AB_FUNC, (intptr_t) Fn->OwnerOverloaded);
			}
			else
				// not found? raise an error, if it's not a safe call
				if (SEqual(Idtf, C4AUL_Inherited) && Type == PARSER)
					throw new C4AulParseError(this, "inherited function not found (use _inherited to disable this message)");
				else
				{
					// otherwise, parse parameters, but discard them
					Parse_Params(0, NULL);
					// Push a null as return value
					AddBCC(AB_STACK, 1);
				}
		}
		else
		{
			// none of these? then it's a function
			C4AulFunc *FoundFn = 0;
			// get regular function
			if (Fn->Owner == a->Engine || !a->GetPropList())
				FoundFn = a->Engine->GetPropList()->GetFunc(Idtf);
			else
			{
				assert(a == Fn->Owner);
				FoundFn = a->GetPropList()->GetFunc(Idtf);
			}
			if (Type == PREPARSER)
			{
				Shift();
				// The preparser just assumes that the syntax is correct: if no '(' follows, it must be a constant
				if (TokenType == ATT_BOPEN)
					Parse_Params(FoundFn ? FoundFn->GetParCount() : 10, Idtf, FoundFn);
			}
			else if (FoundFn)
			{
				if (Config.Developer.ExtraWarnings && !FoundFn->GetPublic())
					Warn("using deprecated function ", Idtf);
				Shift();
				// Function parameters for all functions except "this", which can be used without
				if (!SEqual(FoundFn->GetName(), C4AUL_this) || TokenType == ATT_BOPEN)
					Parse_Params(FoundFn->GetParCount(), FoundFn->GetName(), FoundFn);
				else
					AddBCC(AB_STACK, FoundFn->GetParCount());
				AddBCC(AB_FUNC, (intptr_t) FoundFn);
			}
			else
			{
				// -> func not found
				// check for global constant (static const)
				// global constants have lowest priority for backwards compatibility
				// it is now allowed to have functional overloads of these constants
				C4Value val;
				if (a->Engine->GetGlobalConstant(Idtf, &val))
				{
					// store as direct constant
					switch (val.GetType())
					{
					case C4V_Nil:  AddBCC(AB_NIL,  0); break;
					case C4V_Int:  AddBCC(AB_INT,  val.GetData().Int); break;
					case C4V_Bool: AddBCC(AB_BOOL, val.GetData().Int); break;
					case C4V_String:
						AddBCC(AB_STRING, reinterpret_cast<intptr_t>(val._getStr()));
						break;
					case C4V_PropList:
						AddBCC(AB_CPROPLIST, reinterpret_cast<intptr_t>(val._getPropList()));
						break;
					case C4V_Array:
						AddBCC(AB_CARRAY, reinterpret_cast<intptr_t>(val._getArray()));
						break;
					case C4V_Function:
						AddBCC(AB_CFUNCTION, reinterpret_cast<intptr_t>(val._getFunction()));
						break;
					default:
						throw new C4AulParseError(this,FormatString("internal error: constant %s has unsupported type %d", Idtf, val.GetType()).getData());
					}
					Shift();
				}
				else
				{
					// identifier could not be resolved
					throw new C4AulParseError(this, "unknown identifier: ", Idtf);
				}
			}
		}
		break;
	}
	case ATT_INT: // constant in cInt
	{
		AddBCC(AB_INT, cInt);
		Shift();
		break;
	}
	case ATT_STRING: // reference in cStr
	{
		AddBCC(AB_STRING, reinterpret_cast<intptr_t>(cStr));
		Shift();
		break;
	}
	case ATT_OPERATOR:
	{
		// -> must be a prefix operator
		// get operator ID
		int OpID = cInt;
		// postfix?
		if (C4ScriptOpMap[OpID].Postfix)
			// oops. that's wrong
			throw new C4AulParseError(this, "postfix operator without first expression");
		Shift();
		// generate code for the following expression
		Parse_Expression(C4ScriptOpMap[OpID].Priority);
		C4V_Type to = C4ScriptOpMap[OpID].Type1;
		C4V_Type from = GetLastRetType(to);
		if (C4Value::WarnAboutConversion(from, to))
		{
			Warn(FormatString("operator \"%s\" gets %s instead of %s", C4ScriptOpMap[OpID].Identifier, GetC4VName(from), GetC4VName(to)).getData(), NULL);
		}
		// ignore?
		if (SEqual(C4ScriptOpMap[OpID].Identifier, "+"))
			break;
		// negate constant?
		if (Type == PARSER && SEqual(C4ScriptOpMap[OpID].Identifier, "-"))
			if (a->GetLastCode()->bccType == AB_INT)
			{
				a->GetLastCode()->Par.i = - a->GetLastCode()->Par.i;
				break;
			}
		// changer? make a setter BCC, leave value for operator
		C4AulBCC Changer;
		if(C4ScriptOpMap[OpID].Changer)
			Changer = MakeSetter(true);
		// write byte code
		AddBCC(C4ScriptOpMap[OpID].Code, OpID);
		// writter setter
		if(C4ScriptOpMap[OpID].Changer)
			AddBCC(Changer.bccType, Changer.Par.X);
		break;
	}
	case ATT_BOPEN:
	{
		Shift();
		Parse_Expression();
		Match(ATT_BCLOSE);
		break;
	}
	case ATT_BOPEN2:
	{
		Parse_Array();
		break;
	}
	case ATT_BLOPEN:
	{
		Parse_PropList();
		break;
	}
	default:
	{
		// -> unexpected token
		UnexpectedToken("expression");
	}
	}
	Parse_Expression2(iParentPrio);
}

void C4AulParse::Parse_Expression2(int iParentPrio)
{
	while (1) switch (TokenType)
	{
		case ATT_SET:
		{
			// back out of any kind of parent operator
			// (except other setters, as those are right-associative)
			if(iParentPrio > 1)
				return;
			// generate setter
			C4AulBCC Setter = MakeSetter(false);
			// parse value to set
			Shift();
			Parse_Expression(1);
			// write setter
			AddBCC(Setter.bccType, Setter.Par.X);
			break;
		}
		case ATT_OPERATOR:
		{
			// expect postfix operator
			int OpID = cInt;
			if (!C4ScriptOpMap[OpID].Postfix)
			{
				// does an operator with the same name exist?
				// when it's a postfix-operator, it can be used instead.
				int nOpID;
				for (nOpID = OpID+1; C4ScriptOpMap[nOpID].Identifier; nOpID++)
					if (SEqual(C4ScriptOpMap[OpID].Identifier, C4ScriptOpMap[nOpID].Identifier))
						if (C4ScriptOpMap[nOpID].Postfix)
							break;
				// not found?
				if (!C4ScriptOpMap[nOpID].Identifier)
				{
					throw new C4AulParseError(this, "unexpected prefix operator: ", C4ScriptOpMap[OpID].Identifier);
				}
				// otherwise use the new-found correct postfix operator
				OpID = nOpID;
			}

			// changer?
			C4AulBCC Setter;
			if (C4ScriptOpMap[OpID].Changer)
			{
				// changer: back out only if parent operator is stronger
				// (everything but setters and other changers, as changers are right-associative)
				if(iParentPrio > C4ScriptOpMap[OpID].Priority)
					return;
				// generate setter, leave value on stack for operator
				Setter = MakeSetter(true);
			}
			else
			{
				// normal operator: back out if parent operator is at least as strong
				// (non-setter operators are left-associative)
				if(iParentPrio >= C4ScriptOpMap[OpID].Priority)
					return;
			}
			Shift();

			if (C4ScriptOpMap[OpID].Code == AB_JUMPAND || C4ScriptOpMap[OpID].Code == AB_JUMPOR || C4ScriptOpMap[OpID].Code == AB_JUMPNNIL)
			{
				// create bytecode, remember position
				// Jump or discard first parameter
				int iCond = AddBCC(C4ScriptOpMap[OpID].Code);
				// parse second expression
				Parse_Expression(C4ScriptOpMap[OpID].Priority);
				// set condition jump target
				SetJumpHere(iCond);
				// write setter (unused - could also optimize to skip self-assign, but must keep stack balanced)
				if (C4ScriptOpMap[OpID].Changer)
					AddBCC(Setter.bccType, Setter.Par.X);
				break;
			}
			else
			{
				C4V_Type to = C4ScriptOpMap[OpID].Type1;
				C4V_Type from = GetLastRetType(to);
				if (C4Value::WarnAboutConversion(from, to))
				{
					Warn(FormatString("operator \"%s\" left side gets %s instead of %s", C4ScriptOpMap[OpID].Identifier, GetC4VName(from), GetC4VName(to)).getData(), NULL);
				}
				// expect second parameter for operator
				if (!C4ScriptOpMap[OpID].NoSecondStatement)
					Parse_Expression(C4ScriptOpMap[OpID].Priority);
				to = C4ScriptOpMap[OpID].Type2;
				from = GetLastRetType(to);
				if (C4Value::WarnAboutConversion(from, to))
				{
					Warn(FormatString("operator \"%s\" right side gets %s instead of %s", C4ScriptOpMap[OpID].Identifier, GetC4VName(from), GetC4VName(to)).getData(), NULL);
				}
				// write byte code
				AddBCC(C4ScriptOpMap[OpID].Code, OpID);
				// write setter and mofidier
				if (C4ScriptOpMap[OpID].Changer)
					{
					AddBCC(Setter.bccType, Setter.Par.X);
					if(C4ScriptOpMap[OpID].ResultModifier != AB_ERR)
						AddBCC(C4ScriptOpMap[OpID].ResultModifier, OpID);
					}
			}
			break;
		}
		case ATT_BOPEN2:
		{
			// parse either [index], or [start:end] in which case either index is optional
			Shift();
			if (TokenType == ATT_COLON)
				AddBCC(AB_INT, 0); // slice with first index missing -> implicit start index zero
			else
				Parse_Expression();

			if (TokenType == ATT_BCLOSE2)
			{
				Shift();
				AddBCC(AB_ARRAYA);
			}
			else if (TokenType == ATT_COLON)
			{
				Shift();
				if (TokenType == ATT_BCLOSE2)
				{
					Shift();
					AddBCC(AB_INT, INT_MAX); // second index missing -> implicit end index GetLength()
				}
				else
				{
					Parse_Expression();
					Match(ATT_BCLOSE2);
				}
				AddBCC(AB_ARRAY_SLICE);
			}
			else
			{
				UnexpectedToken("']' or ':'");
			}
			break;
		}
		case ATT_DOT:
		{
			Shift();
			if (TokenType != ATT_IDTF)
				UnexpectedToken("Identifier");
			C4String * pKey = Strings.RegString(Idtf);
			AddBCC(AB_PROP, (intptr_t) pKey);
			Shift();
			break;
		}
		case ATT_CALL: case ATT_CALLFS:
		{
			C4AulFunc *pFunc = NULL;
			C4String *pName = NULL;
			C4AulBCCType eCallType = (TokenType == ATT_CALL) ? AB_CALL : AB_CALLFS;
			Shift();
			// expect identifier of called function now
			if (TokenType != ATT_IDTF) throw new C4AulParseError(this, "expecting func name after '->'");
			pFunc = a->Engine->GetFirstFunc(Idtf);
			if (Type == PARSER)
				pName = ::Strings.RegString(Idtf);
			Shift();
			Parse_Params(C4AUL_MAX_Par, pName ? pName->GetCStr() : Idtf, pFunc);
			AddBCC(eCallType, reinterpret_cast<intptr_t>(pName));
			break;
		}
		default:
		{
			return;
		}
	}
}

void C4AulParse::Parse_Var()
{
	Shift();
	while (1)
	{
		// get desired variable name
		if (TokenType != ATT_IDTF)
			UnexpectedToken("variable name");
		if (Type == PREPARSER)
		{
			// insert variable
			Fn->VarNamed.AddName(Idtf);
		}
		// search variable (fail if not found)
		int iVarID = Fn->VarNamed.GetItemNr(Idtf);
		if (iVarID < 0)
			throw new C4AulParseError(this, "internal error: var definition: var not found in variable table");
		Shift();
		if(TokenType == ATT_SET)
		{
			// insert initialization in byte code
			Shift();
			Parse_Expression();
			AddBCC(AB_POP_TO, 1 + iVarID - (iStack + Fn->VarNamed.iSize));
		}
		switch (TokenType)
		{
		case ATT_COMMA:
			Shift();
			break;
		case ATT_SCOLON:
			return;
		default:
			UnexpectedToken("',' or ';'");
		}
	}
}

void C4AulParse::Parse_Local()
{
	Shift();
	while (1)
	{
		if (Type == PREPARSER)
		{
			// get desired variable name
			if (TokenType != ATT_IDTF)
				UnexpectedToken("variable name");
			// check: symbol already in use?
			if (a->GetPropList() && a->GetPropList()->GetFunc(Idtf))
				throw new C4AulParseError(this, "variable definition: name already in use");
			// insert variable
			a->LocalNamed.AddName(Idtf);
		}
		char Name[C4AUL_MAX_Identifier] = ""; // current identifier
		SCopy(Idtf, Name);
		Match(ATT_IDTF);
		if (TokenType == ATT_SET)
		{
			if (!a->GetPropList())
				throw new C4AulParseError(this, "local variables can only be initialized on proplists");
			Shift();
			C4RefCntPointer<C4String> key = ::Strings.RegString(Name);
			assert(a->GetPropList()->IsStatic());
			Parse_ConstExpression(a->GetPropList()->IsStatic(), key);
		}
		switch (TokenType)
		{
		case ATT_COMMA:
		{
			Shift();
			break;
		}
		case ATT_SCOLON:
		{
			return;
		}
		default:
		{
			UnexpectedToken("',' or ';'");
		}
		}
	}
}

void C4AulParse::Parse_Static()
{
	Shift();
	// constant?
	if (TokenType == ATT_IDTF && SEqual(Idtf, C4AUL_Const))
	{
		Parse_Const();
		return;
	}
	while (1)
	{
		if (Type == PREPARSER)
		{
			// get desired variable name
			if (TokenType != ATT_IDTF)
				UnexpectedToken("variable name");
			// global variable definition
			// check: symbol already in use?
			if (a->Engine->GetPropList()->GetFunc(Idtf)) Error("function and variable with name ", Idtf);
			if (a->Engine->GetGlobalConstant(Idtf, NULL)) Error("constant and variable with name ", Idtf);
			// insert variable if not defined already
			if (a->Engine->GlobalNamedNames.GetItemNr(Idtf) == -1)
			{
				a->Engine->GlobalNamedNames.AddName(Idtf);
			}
		}
		Match(ATT_IDTF);
		switch (TokenType)
		{
		case ATT_COMMA:
		{
			Shift();
			break;
		}
		case ATT_SCOLON:
		{
			return;
		}
		default:
		{
			UnexpectedToken("',' or ';'");
		}
		}
	}
}

C4Value C4AulParse::Parse_ConstExpression(C4PropListStatic * parent, C4String * Name)
{
	C4Value r;
	switch (TokenType)
	{
	case ATT_INT: r.SetInt(cInt); break;
	case ATT_STRING: r.SetString(cStr); break; // increases ref count of C4String in cStr
	case ATT_IDTF:
		// identifier is only OK if it's another constant
		if (SEqual(Idtf, C4AUL_True))
			r.SetBool(true);
		else if (SEqual(Idtf, C4AUL_False))
			r.SetBool(false);
		else if (SEqual(Idtf, C4AUL_Nil))
			r.Set0();
		else if (SEqual(Idtf, C4AUL_New))
			r = Parse_ConstPropList(parent, Name);
		else if (!a->Engine->GetGlobalConstant(Idtf, &r))
			UnexpectedToken("constant value");
		break;
	case ATT_BOPEN2:
		{
			Shift();
			// Create an array
			if (Type == PARSER)
				r.SetArray(new C4ValueArray());
			int size = 0;
			bool fDone = false;
			do
			switch (TokenType)
			{
				case ATT_BCLOSE2:
				{
					// [] -> size 0, [*,] -> size 2, [*,*,] -> size 3
					if (size > 0)
					{
						if (Type == PARSER)
							r._getArray()->SetItem(size, C4VNull);
						++size;
					}
					fDone = true;
					break;
				}
				case ATT_COMMA:
				{
					// got no parameter before a ","? then push nil
					if (Type == PARSER)
						r._getArray()->SetItem(size, C4VNull);
					Shift();
					++size;
					break;
				}
				default:
				{
					if (Type == PARSER)
						r._getArray()->SetItem(size, Parse_ConstExpression(NULL, NULL));
					else
						Parse_ConstExpression(NULL, NULL);
					++size;
					if (TokenType == ATT_COMMA)
						Shift();
					else if (TokenType == ATT_BCLOSE2)
					{
						fDone = true;
						break;
					}
					else
						UnexpectedToken("',' or ']'");
				}
			}
			while (!fDone);
			break;
		}
	case ATT_BLOPEN:
		{
			r = Parse_ConstPropList(parent, Name);
			break;
		}
	case ATT_OPERATOR:
		{
			// -> must be a prefix operator
			// get operator ID
			int OpID = cInt;
			if (SEqual(C4ScriptOpMap[OpID].Identifier, "+"))
			{
				Shift();
				if (TokenType == ATT_INT)
				{
					r.SetInt(cInt); break;
				}
			}
			if (SEqual(C4ScriptOpMap[OpID].Identifier, "-"))
			{
				Shift();
				if (TokenType == ATT_INT)
				{
					r.SetInt(-cInt); break;
				}
			}
		}
		// fallthrough
	default:
		UnexpectedToken("constant value");
	}
	// expect ',' (next global) or ';' (end of definition) now
	Shift();
	if (TokenType == ATT_OPERATOR)
	{
		int OpID = cInt;
		if (C4ScriptOpMap[OpID].Code == AB_BitOr)
		{
			Shift();
			C4Value r2 = Parse_ConstExpression(NULL, NULL);
			r.SetInt(r.getInt() | r2.getInt());
		}
	}
	// store as constant or property
	if (Name)
	{
		if (parent)
			parent->SetPropertyByS(Name, r);
		else
			a->Engine->RegisterGlobalConstant(Name->GetCStr(), r);
	}
	return r;
}

void C4AulParse::Parse_Const()
{
	Shift();
	// get global constant definition(s)
	while (1)
	{
		char Name[C4AUL_MAX_Identifier] = ""; // current identifier
		// get desired variable name
		if (TokenType != ATT_IDTF)
			UnexpectedToken("constant name");
		SCopy(Idtf, Name);
		// check func lists - functions of same name are not allowed
		if (a->Engine->GetPropList()->GetFunc(Idtf))
			Error("definition of constant hidden by function ", Idtf);
		if (a->Engine->GlobalNamedNames.GetItemNr(Idtf) != -1)
			Error("constant and variable with name ", Idtf);
		Match(ATT_IDTF);
		// expect '='
		if (TokenType != ATT_SET)
			UnexpectedToken("'='");
		// expect value. Theoretically, something like C4AulScript::ExecOperator could be used here
		// this would allow for definitions like "static const OCF_CrewMember = 1<<20"
		// However, such stuff should better be generalized, so the preparser (and parser)
		// can evaluate any constant expression, including functions with constant retval (e.g. Sqrt)
		// So allow only simple constants for now.
		Shift();

		C4RefCntPointer<C4String> key = ::Strings.RegString(Name);
		Parse_ConstExpression(NULL, key);

		switch (TokenType)
		{
			case ATT_COMMA:
			{
				Shift();
				break;
			}
			case ATT_SCOLON:
			{
				return;
			}
			default:
			{
				UnexpectedToken("',' or ';'");
			}
		}
	}
}

bool C4AulScript::Parse()
{
	return true;
}

bool C4ScriptHost::Parse()
{
	if (DEBUG_BYTECODE_DUMP)
	{
		fprintf(stderr, "parsing %s...\n", ScriptName.getData());
	}
	// check state
	if (State != ASS_LINKED) return false;

	// delete existing code
	ClearCode();

	if (!Appends.empty())
	{
		// #appendto scripts are not allowed to contain global functions or belong to definitions
		// so their contents are not reachable
		return true;
	}

	C4AulFunc * thisfuncs = Func0;
	for (C4AulFunc *f = thisfuncs; f; f = f->Next)
		if (f->GetName() && f->SFunc())
		{
			Engine->FuncLookUp.Remove(f);
		}
	Func0 = FuncL = 0;

	for (std::list<C4ScriptHost *>::iterator s = SourceScripts.begin(); s != SourceScripts.end(); ++s)
	{
		if (*s == this)
		{
			C4AulFunc *f = thisfuncs, *n;
			while (f)
			{
				n = f->Next;
				f->AppendToScript(this);
				f = n;
			}
			continue;
		}
		// definition appends
		if (GetPropList() && GetPropList()->GetDef() && (*s)->GetPropList() && (*s)->GetPropList()->GetDef())
			GetPropList()->GetDef()->IncludeDefinition((*s)->GetPropList()->GetDef());
		// append all funcs
		C4AulScriptFunc *sf;
		for (C4AulFunc *f = (*s)->Func0; f; f = f->Next)
			// funcs from other scripts get appended directly from their script
			if ((sf = f->SFunc()) && sf->pOrgScript == *s)
			{
				// append: create copy
				C4AulScriptFunc *sfc = new C4AulScriptFunc(this, *sf);
			}
		// copy local var definitions
		for (int ivar = 0; ivar < (*s)->LocalNamed.iSize; ivar ++)
			LocalNamed.AddName((*s)->LocalNamed.pNames[ivar]);
	}

	LinkFunctions();

	// parse
	C4AulParse state(this, C4AulParse::PARSER);
	for (std::list<C4ScriptHost *>::iterator s = SourceScripts.begin(); s != SourceScripts.end(); ++s)
	{
		state.SPos = (*s)->Script.getData();
		state.pOrgScript = *s;
		state.Parse_Script();
	}

	// add eof chunk
	AddBCC(AB_EOF);

	// calc absolute code addresses for script funcs
	for (C4AulFunc * f = Func0; f; f = f->Next)
	{
		C4AulScriptFunc * Fn = f->SFunc();
		assert(Fn);
		if (Fn)
			assert(Fn->GetCodeOwner() == this);
	}

	// save line count
	Engine->lineCnt += SGetLine(Script.getData(), Script.getPtr(Script.getLength()));

	// dump bytecode
	if (DEBUG_BYTECODE_DUMP)
		for (C4AulFunc * f = Func0; f; f = f->Next)
		{
			C4AulScriptFunc *Fn = f->SFunc();
			assert(Fn);
			if (!Fn)
				continue;
			fprintf(stderr, "%s:\n", Fn->GetName());
			std::map<C4AulBCC *, int> labels;
			int labeln = 0;
			for (C4AulBCC *pBCC = Fn->GetCode(); pBCC->bccType != AB_EOFN; pBCC++)
			{
				switch (pBCC->bccType)
				{
				case AB_JUMP: case AB_JUMPAND: case AB_JUMPOR: case AB_JUMPNNIL: case AB_CONDN: case AB_COND:
					labels[pBCC + pBCC->Par.i] = ++labeln; break;
				default: break;
				}
			}
			for (C4AulBCC *pBCC = Fn->GetCode();; pBCC++)
			{
				C4AulBCCType eType = pBCC->bccType;
				if (labels.find(pBCC) != labels.end())
					fprintf(stderr, "%d:\n", labels[pBCC]);
				fprintf(stderr, "\t%d\t%s", Fn->GetLineOfCode(pBCC), GetTTName(eType));
				switch (eType)
				{
				case AB_FUNC:
					fprintf(stderr, "\t%s\n", pBCC->Par.f->GetName()); break;
				case AB_CALL: case AB_CALLFS: case AB_LOCALN: case AB_LOCALN_SET: case AB_PROP: case AB_PROP_SET:
					fprintf(stderr, "\t%s\n", pBCC->Par.s->GetCStr()); break;
				case AB_STRING:
					fprintf(stderr, "\t\"%s\"\n", pBCC->Par.s->GetCStr()); break;
				case AB_DEBUG: case AB_NIL: case AB_RETURN:
				case AB_PAR:
				case AB_ARRAYA: case AB_ARRAYA_SET: case AB_ARRAY_SLICE: case AB_ARRAY_SLICE_SET:
				case AB_ERR: case AB_EOFN: case AB_EOF:
					assert(!pBCC->Par.X); fprintf(stderr, "\n"); break;
				case AB_CARRAY: case AB_CPROPLIST:
					fprintf(stderr, "\t%p\n", reinterpret_cast<void *>(pBCC->Par.X)); break;
				case AB_JUMP: case AB_JUMPAND: case AB_JUMPOR: case AB_JUMPNNIL: case AB_CONDN: case AB_COND:
					fprintf(stderr, "\t%d\n", labels[pBCC + pBCC->Par.i]); break;
				default:
					fprintf(stderr, "\t%d\n", pBCC->Par.i); break;
				}
				if (eType == AB_EOFN) break;
			}
		}

	// finished
	State = ASS_PARSED;

	return true;
}

#undef DEBUG_BYTECODE_DUMP
