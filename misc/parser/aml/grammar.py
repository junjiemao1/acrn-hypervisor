# Primary opcodes
AML_ZERO_OP                 = 0x00
AML_ONE_OP                  = 0x01
AML_ALIAS_OP                = 0x06
AML_NAME_OP                 = 0x08
AML_BYTE_PREFIX             = 0x0A
AML_WORD_PREFIX             = 0x0B
AML_DWORD_PREFIX            = 0x0C
AML_STRING_PREFIX           = 0x0D
AML_QWORD_PREFIX            = 0x0E
AML_SCOPE_OP                = 0x10
AML_BUFFER_OP               = 0x11
AML_PACKAGE_OP              = 0x12
AML_VAR_PACKAGE_OP          = 0x13
AML_METHOD_OP               = 0x14
AML_EXTERNAL_OP             = 0x15
AML_DUAL_NAME_PREFIX        = 0x2E
AML_MULTI_NAME_PREFIX       = 0x2F
AML_EXT_OP_PREFIX           = 0x5B
AML_FIRST_LOCAL_OP          = 0x60
AML_LOCAL0_OP               = 0x60
AML_LOCAL1_OP               = 0x61
AML_LOCAL2_OP               = 0x62
AML_LOCAL3_OP               = 0x63
AML_LOCAL4_OP               = 0x64
AML_LOCAL5_OP               = 0x65
AML_LOCAL6_OP               = 0x66
AML_LOCAL7_OP               = 0x67
AML_ARG0_OP                 = 0x68
AML_ARG1_OP                 = 0x69
AML_ARG2_OP                 = 0x6A
AML_ARG3_OP                 = 0x6B
AML_ARG4_OP                 = 0x6C
AML_ARG5_OP                 = 0x6D
AML_ARG6_OP                 = 0x6E
AML_STORE_OP                = 0x70
AML_REF_OF_OP               = 0x71
AML_ADD_OP                  = 0x72
AML_CONCAT_OP               = 0x73
AML_SUBTRACT_OP             = 0x74
AML_INCREMENT_OP            = 0x75
AML_DECREMENT_OP            = 0x76
AML_MULTIPLY_OP             = 0x77
AML_DIVIDE_OP               = 0x78
AML_SHIFT_LEFT_OP           = 0x79
AML_SHIFT_RIGHT_OP          = 0x7A
AML_AND_OP                  = 0x7B
AML_NAND_OP                 = 0x7C
AML_OR_OP                   = 0x7D
AML_NOR_OP                  = 0x7E
AML_XOR_OP                  = 0x7F
AML_NOT_OP                  = 0x80
AML_FIND_SET_LEFT_BIT_OP    = 0x81
AML_FIND_SET_RIGHT_BIT_OP   = 0x82
AML_DEREF_OF_OP             = 0x83
AML_CONCAT_RES_OP           = 0x84
AML_MOD_OP                  = 0x85
AML_NOTIFY_OP               = 0x86
AML_SIZE_OF_OP              = 0x87
AML_INDEX_OP                = 0x88
AML_MATCH_OP                = 0x89
AML_CREATE_DWORD_FIELD_OP   = 0x8A
AML_CREATE_WORD_FIELD_OP    = 0x8B
AML_CREATE_BYTE_FIELD_OP    = 0x8C
AML_CREATE_BIT_FIELD_OP     = 0x8D
AML_OBJECT_TYPE_OP          = 0x8E
AML_CREATE_QWORD_FIELD_OP   = 0x8F
AML_LAND_OP                 = 0x90
AML_LOR_OP                  = 0x91
AML_LNOT_OP                 = 0x92
AML_LEQUAL_OP               = 0x93
AML_LGREATER_OP             = 0x94
AML_LLESS_OP                = 0x95
AML_TO_BUFFER_OP            = 0x96
AML_TO_DECIMAL_STRING_OP    = 0x97
AML_TO_HEX_STRING_OP        = 0x98
AML_TO_INTEGER_OP           = 0x99
AML_TO_STRING_OP            = 0x9C
AML_COPY_OBJECT_OP          = 0x9D
AML_MID_OP                  = 0x9E
AML_CONTINUE_OP             = 0x9F
AML_IF_OP                   = 0xA0
AML_ELSE_OP                 = 0xA1
AML_WHILE_OP                = 0xA2
AML_NOOP_OP                 = 0xA3
AML_RETURN_OP               = 0xA4
AML_BREAK_OP                = 0xA5
AML_BREAKPOINT_OP           = 0xCC
AML_ONES_OP                 = 0xFF

# Prefixed opcodes, with the least byte being AML_EXT_OP_PREFIX
AML_MUTEX_OP                = 0x015b
AML_EVENT_OP                = 0x025b
AML_CONDITIONAL_REF_OF_OP   = 0x125b
AML_CREATE_FIELD_OP         = 0x135b
AML_LOAD_TABLE_OP           = 0x1f5b
AML_LOAD_OP                 = 0x205b
AML_STALL_OP                = 0x215b
AML_SLEEP_OP                = 0x225b
AML_ACQUIRE_OP              = 0x235b
AML_SIGNAL_OP               = 0x245b
AML_WAIT_OP                 = 0x255b
AML_RESET_OP                = 0x265b
AML_RELEASE_OP              = 0x275b
AML_FROM_BCD_OP             = 0x285b
AML_TO_BCD_OP               = 0x295b
AML_UNLOAD_OP               = 0x2a5b
AML_REVISION_OP             = 0x305b
AML_DEBUG_OP                = 0x315b
AML_FATAL_OP                = 0x325b
AML_TIMER_OP                = 0x335b
AML_REGION_OP               = 0x805b
AML_FIELD_OP                = 0x815b
AML_DEVICE_OP               = 0x825b
AML_PROCESSOR_OP            = 0x835b
AML_POWER_RESOURCE_OP       = 0x845b
AML_THERMAL_ZONE_OP         = 0x855b
AML_INDEX_FIELD_OP          = 0x865b
AML_BANK_FIELD_OP           = 0x875b
AML_DATA_REGION_OP          = 0x885b


# 20.2.1 Table and Table Header Encoding

AMLCode = ("DefBlockHeader", "TermObj*")
DefBlockHeader = ("TableSignature", "TableLength", "SpecCompliance", "CheckSum", "OemID", "OemTableID", "OemRevision", "CreatorID", "CreatorRevision")
TableSignature = ("DWordData",)
TableLength = ("DWordData",)
SpecCompliance = ("ByteData",)
CheckSum = ("ByteData",)
OemID = ("TWordData",)
OemTableID = ("QWordData",)
OemRevision = ("DWordData",)
CreatorID = ("DWordData",)
CreatorRevision = ("DWordData",)

# 20.2.2 Name Objects Encoding

# 20.2.3 Data Objects Encoding

ComputationalData = ["ByteConst", "WordConst", "DWordConst", "QWordConst", "ConstObj"]
DataObject = ["ComputationalData"]
DataRefObject = ["DataObject"]

ByteConst = (AML_BYTE_PREFIX, "ByteData")
WordConst = (AML_WORD_PREFIX, "WordData")
DWordConst = (AML_DWORD_PREFIX, "DWordData")
QWordConst = (AML_QWORD_PREFIX, "QWordData")

ConstObj = ["ZeroOp", "OneOp", "OnesOp"]
ZeroOp = (AML_ZERO_OP,)
OneOp = (AML_ONE_OP,)
OnesOp = (AML_ONES_OP,)

# 20.2.4 Package Length Encoding

# 20.2.5 Term Objects Encoding

Object = ["NameSpaceModifierObj", "NamedObj"]
TermObj = ["Object", "StatementOpcode"]
TermList = ("TermObj*",)
TermArg = ["DataObject"]

# 20.2.5.1 Namespace Modifier Objects Encoding

NameSpaceModifierObj = ["DefName"]

DefName = (AML_NAME_OP, "NameString", "DataRefObject")

# 20.2.5.2 Named Objects Encoding

NamedObj = ["DefExternal", "DefOpRegion", "DefField"]

DefExternal = (AML_EXTERNAL_OP, "NameString", "ObjectType", "ArgumentCount")
ObjectType = ("ByteData",)
ArgumentCount = ("ByteData",)

DefOpRegion = (AML_REGION_OP, "NameString", "TermArg", "TermArg", "TermArg")

DefField = (AML_FIELD_OP, "PkgLength", "NameString", "FieldFlags", "FieldList")
FieldFlags = ("ByteData",)
FieldList = ("FieldElement*",)

FieldElement = []

# 20.2.5.3 Statement Opcodes Encoding

StatementOpcode = ["DefIfElse"]

DefIfElse = (AML_IF_OP, "PkgLength", "Predicate", "TermList")
Predicate = ["TermArg"]

# 20.2.5.4 Expression Opcodes Encoding

# 20.2.6.1 Arg Objects Encoding

# 20.2.6.2 Local Objects Encoding

# 20.2.6.3 Debug Objects Encoding
