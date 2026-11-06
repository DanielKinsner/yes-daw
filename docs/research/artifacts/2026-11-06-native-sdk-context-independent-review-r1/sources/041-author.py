"""Author complete selected source-incidence data; never import or run targets."""
from pathlib import Path
import hashlib
import json
import re

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
OLD=ROOT/'build-ci/20261106-native-controlled-object-envelope-r1'
SDK=Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include')
UCRT=Path('C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def put(n,v): (OUT/n).write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8',newline='\n')
if (OUT/'first-green-freeze.json').exists(): raise SystemExit('FROZEN')
attemptroot=OUT/'author-attempts';attemptroot.mkdir(exist_ok=True)
attempt=attemptroot/('attempt-'+str(len(list(attemptroot.iterdir()))+1).zfill(2));attempt.mkdir()
for p in OUT.iterdir():
    if p.is_file(): (attempt/p.name).write_bytes(p.read_bytes())
oldpins=json.loads((OLD/'source-pins.json').read_text(encoding='utf-8'))
env=json.loads((OLD/'envelope.json').read_text(encoding='utf-8'))
closures=json.loads((OLD/'closure-contexts.json').read_text(encoding='utf-8'))['closures']
sources={x['alias']: ROOT/x['path'] for x in env['source_inputs']}
for name in ['vector','xmemory','xutility','xstring','array','utility','type_traits','bit','limits','initializer_list','exception','stdexcept','vcruntime_exception.h','yvals.h','yvals_core.h','__msvc_string_view.hpp','__msvc_iter_core.hpp','__msvc_sanitizer_annotate_container.hpp','cmath']:
    sources[name]=SDK/name
sources['corecrt_math.h']=UCRT/'corecrt_math.h'
text={k:p.read_text(encoding='utf-8-sig') for k,p in sources.items()}

# Row fields: ID | source | inclusive lines | entered declaration | value formals |
# references/aliases | declaration locals | materializations | actual selected children.
# Objects are type:name[,name] separated by semicolons. Unknown types have named symbols.
# No typename, unevaluated operand or compile-time-only selector is a runtime object.
SPEC=r'''
V_DEFAULT|vector|670-672|vector()|none|receiver|none|none|C_ZERO,FA_ALLOC_PROXY
V_ILIST|vector|719-722|vector(initializer_list|IL:_Ilist|_Al const allocator&|none|ALLOC_CHANGE:default_Al|C_ONE,I_SIZE,I_BEGIN,I_END,CONVERT,V_CONSTRUCT
V_COPY|vector|745-750|vector(const vector&|none|_Right;_Right_data|Z:_Count|ALLOC_CHANGE:returned_allocator|ALLOC_SELECT,C_ONE,V_CONSTRUCT
V_MOVE|vector|759-766|vector(vector&&|none|_Right|none|NUL:exchange_null1,exchange_null2,exchange_null3;P:exchange_result1,exchange_result2,exchange_result3|C_ONE,EXCHANGE,EXCHANGE,EXCHANGE,V_VAL_CTOR,FA_ALLOC_PROXY,FA_SWAP
V_MOVE_ASSIGN|vector|798-827|operator=(vector&&|none|_Right;_Al;_Right_al|none|none|ADDRESS,V_TIDY,POCMA,V_TAKE
V_DTOR|vector|829-836|~vector()|none|receiver|none|none|V_TIDY
V_CONSTRUCT|vector|2102-2130|_Construct_n(|Z:_Count|_Val...;_Al;_Alproxy;_My_data|FA:_Alproxy_temporary;FP:_Proxy;TG:_Guard|none|FP_CTOR,V_BUY_NONZERO,UNINIT_COPY,FP_RELEASE
V_BUY_NONZERO|vector|2040-2056|_Buy_nonzero(|Z:_Newcapacity|receiver|none|none|V_MAX,V_BUY_RAW
V_BUY_RAW|vector|2024-2038|_Buy_raw(|Z:_Newcapacity|_My_data;_Myfirst;_Mylast;_Myend|P:_Newvec|none|ALLOC_AT_LEAST
V_ASSIGN_FILL|vector|1335-1390|void assign(|Z:_Newsize|_Val;_Al;_My_data;_Myfirst;_Mylast|Z:_Oldcapacity|D:zero_fill_argument|V_CLEAR,UNINIT_FILL
V_ASSIGN_RANGE|vector|1484-1501|void assign(_Iter|P:_First,_Last|receiver|P:_UFirst,_ULast;Z:_Length,_Count|none|VERIFY,UNWRAP,UNWRAP,DISTANCE,CONVERT,V_ASSIGN_COUNTED
V_ASSIGN_COUNTED|vector|1420-1480|_Assign_counted_range(|P:_First;Z:_Newsize|_Al;_My_data;_Myfirst;_Mylast;_Myend|Z:_Oldcapacity|none|V_CLEAR,UNINIT_COPY_N
V_CLEAR|vector|1692-1720|_Clear_and_reserve_geometric(|Z:_Newsize|_Al;_My_data;_Myfirst;_Mylast;_Myend|Z:_Newcapacity|none|V_MAX,V_GROWTH,V_BUY_RAW
V_GROWTH|vector|2005-2022|_Calculate_growth(|Z:_Newsize|receiver|Z:_Oldcapacity,_Max,_Geometric|none|V_CAPACITY,V_MAX
V_MAX|vector|1918-1921|max_size()|none|receiver|none|Z:min_left,min_right|MAX_LIMIT,ALLOC_MAX,MINMAX
V_CAPACITY|vector|1922-1925|capacity()|none|_My_data|none|none|none
V_TIDY|vector|2081-2100|_Tidy()|none|_Al;_My_data;_Myfirst;_Mylast;_Myend|none|none|FA_ORPHAN,DESTROY_RANGE,ALLOC_DEALLOC
V_TAKE|vector|425-434|_Take_contents(|none|_Right|none|none|FA_SWAP
V_VAL_CTOR|vector|400-403|_Vector_val(pointer|P:_First,_Last,_End|receiver|none|none|none
V_ACCESS|vector|1908-1947|operator[]|Z:_Pos_if_subscript|_My_data|none|none|none
V_BEGINEND|vector|1840-1858|const_iterator begin()|none|_My_data|none|none|ADDRESS,V_ITER_CTOR
V_ITER_CTOR|vector|46-48|_Vector_const_iterator(_Tptr|P:_Parg,_Pvector|receiver|none|none|FA_ADOPT
V_ITER_UNWRAP|vector|221-229|_Unwrapped()|P:_It_if_seek|receiver|none|none|UNFANCY,REFANCY
V_ITER_DIFFERENCE|vector|154-157|operator-(const _Vector_const_iterator&|none|_Right|none|none|V_COMPAT
V_ITER_EQ|vector|164-167|operator==|none|_Right|none|none|V_COMPAT
V_COMPAT|vector|199-208|_Compat(|none|_Right|none|none|none
C_ZERO|xmemory|1528-1532|_Compressed_pair(_Zero|TAG_ZERO:unnamed_tag|_Val2...|none|none|FORWARD
C_ONE|xmemory|1533-1537|_Compressed_pair(_One|TAG_ONE:unnamed_tag|_Val1;_Val2...|none|none|FORWARD
FA_ALLOC_PROXY|xmemory|1183-1188|_Alloc_proxy(|none|_Fake_allocator const&|none|FA:fake_allocator_argument|none
FA_SWAP|xmemory|1183-1188|_Swap_proxy_and_iterators|none|other_container|none|none|none
FA_ORPHAN|xmemory|1183-1188|_Orphan_all()|none|receiver|none|none|none
FA_ADOPT|xmemory|1191-1193|_Adopt(|P:unnamed_container_pointer|receiver|none|none|none
FP_CTOR|xmemory|1446-1451|_Fake_proxy_ptr_impl(const _Fake_allocator&, const _Container_base0&)|none|allocator;container|none|none|none
FP_RELEASE|xmemory|1452-1453|_Release()|none|receiver|none|none|none
TG_DTOR|xmemory|84-89|~_Tidy_guard()|none|receiver|none|none|none
ALLOC_SELECT|xmemory|749-751|select_on_container_copy_construction|none|_Al|none|none|none
ALLOC_MAX|xmemory|745-747|max_size(const _Alloc&)|none|allocator|none|none|none
ALLOC_AT_LEAST|xmemory|2293-2305|_Allocate_at_least_helper(|none|_Al;_Count|none|none|ALLOC_ALLOCATE
ALLOC_ALLOCATE|xmemory|988-991|allocate(_CRT_GUARDOVERFLOW|Z:_Count|receiver|none|none|SIZE_OF_N,ALLOC_BYTES
SIZE_OF_N|xmemory|112-124|_Get_size_of_n(|Z:_Count|none|none|none|none
ALLOC_BYTES|xmemory|225-260|void* _Allocate(|Z:_Bytes|none|none|none|DEFAULT_NEW
DEFAULT_NEW|xmemory|135-137|void* _Allocate(|Z:_Bytes|none|none|none|PRIMITIVE
ALLOC_DEALLOC|xmemory|982-986|void deallocate(|P:_Ptr;Z:_Count|receiver|none|none|DEALLOC_BYTES
DEALLOC_BYTES|xmemory|262-292|void _Deallocate(|P:_Ptr;Z:_Bytes|none|none|none|PRIMITIVE
POCMA|xmemory|1082-1086|_Pocma(|none|_Left;_Right|none|none|MOVE
UNINIT_COPY|xmemory|1860-1906|_Uninitialized_copy(|P:_First,_Last,_Dest|_Al|P:_UFirst,_ULast|none|RANGE_UNWRAP,RANGE_UNWRAP,COPY_MEMMOVE,TO_ADDRESS,UNFANCY
UNINIT_COPY_N|xmemory|1909-1943|_Uninitialized_copy_n(|P:_First,_Dest;Z:_Count|_Al|P:_UFirst|none|UNWRAP,COPY_MEMMOVE_N,UNFANCY
UNINIT_FILL|xmemory|2006-2034|_Uninitialized_fill_n(|P:_First;Z:_Count|_Val;_Al|none|none|BITS_ZERO,FILL_ZERO,UNFANCY
DESTROY_RANGE|xmemory|1098-1107|_Destroy_range(|P:_First,_Last|_Al|none|none|none
CONVERT|xmemory|1119-1133|_Convert_size(|Z:_Len|none|none|none|none
UNFANCY|xmemory|66-68|_Unfancy(_Ty*|P:_Ptr|none|none|none|none
REFANCY|xmemory|312-314|_Refancy_maybe_null(_Pointer|P:_Ptr|none|none|none|none
CONSTRUCT_IN_PLACE|xutility|465-478|_Construct_in_place(|none|_Obj;_Types...|none|none|FORWARD
DESTROY_IN_PLACE|xmemory|320-326|_Destroy_in_place(|none|_Obj|none|none|none
S_CSTR|xstring|768-770|basic_string(_In_z_|P:_Ptr|receiver|none|none|C_ZERO,TRAITS_LENGTH,CONVERT,S_CONSTRUCT
S_PTRCOUNT|xstring|754-757|basic_string(_In_reads_|P:_Ptr;Z:_Count|receiver|none|none|C_ZERO,S_CONSTRUCT
S_CONSTRUCT|xstring|871-931|void _Construct(|P:_Arg;Z:_Count|_My_data;_Al;_Alproxy|FA:_Alproxy_temporary;FP:_Proxy;Z:_New_capacity;P:_New_ptr|CH:terminating_zero|S_MAX,FP_CTOR,S_GROWTH_STATIC,S_ALLOC_CAP,CONSTRUCT_IN_PLACE,TRAITS_COPY,TRAITS_ASSIGN,FP_RELEASE
S_MOVE|xstring|1027-1031|basic_string(basic_string&&|none|_Right|none|none|C_ONE,MOVE,FA_ALLOC_PROXY,S_TAKE
S_TAKE|xstring|1258-1295|void _Take_contents(|none|_Right;_My_data;_Right_data|P:_My_data_mem,_Right_data_mem|CH:terminating_zero|ADDRESS,S_ACTIVATE,TRAITS_ASSIGN,PRIMITIVE
S_CONCAT_PTRS|xstring|1048-1077|basic_string(_String_constructor_concat_tag, const basic_string&|TAG_CONCAT:unnamed_tag;P:_Left_ptr,_Right_ptr;Z:_Left_size,_Right_size|_Source_of_al;_My_data;_Alproxy|Z:_New_size,_New_capacity;P:_Ptr,_Fancyptr;FA:_Alproxy_temporary;FP:_Proxy|ALLOC_CHAR:returned_allocator;CH:terminating_zero|C_ONE,ALLOC_SELECT,FP_CTOR,S_GROWTH_STATIC,S_ALLOC_CAP,CONSTRUCT_IN_PLACE,UNFANCY,TRAITS_COPY,TRAITS_ASSIGN,FP_RELEASE
S_CONCAT_RR|xstring|1079-1145|basic_string(_String_constructor_concat_tag, basic_string&|TAG_CONCAT:unnamed_tag|_Left;_Right;_My_data;_Left_data;_Right_data;_Alproxy|Z:_Left_size,_Right_size,_Left_capacity,_Right_capacity,_New_size,_Max,_New_capacity;B:_Fits_in_left,_Fits_in_right;P:_Ptr_branch_left,_Ptr_branch_right,_Fancyptr,_Ptr_branch_allocate;FA:_Alproxy_temporary;FP:_Proxy|none|C_ONE,FA_ORPHAN,FA_ALLOC_PROXY,S_TAKE,S_PTR,TRAITS_COPY,ALLOC_EQUAL,TRAITS_MOVE,S_MAX,S_GROWTH_STATIC,FP_CTOR,S_ALLOC_CAP,CONSTRUCT_IN_PLACE,UNFANCY,FP_RELEASE
S_PLUS_LL|xstring|3147-3156|const basic_string<_Elem, _Traits, _Alloc>& _Left, const basic_string<_Elem, _Traits, _Alloc>& _Right)|none|_Left;_Right|Z:_Left_size,_Right_size|none|S_SIZE,S_MAX,S_CSTR_GET,S_CONCAT_PTRS
S_PLUS_PL|xstring|3159-3169|_In_z_ const _Elem* const _Left, const basic_string|P:_Left|_Right|Z:_Left_size,_Right_size|none|TRAITS_LENGTH,CONVERT,S_SIZE,S_MAX,S_CSTR_GET,S_CONCAT_PTRS
S_PLUS_LP|xstring|3183-3193|const basic_string<_Elem, _Traits, _Alloc>& _Left, _In_z_ const _Elem* const _Right)|P:_Right|_Left|Z:_Left_size,_Right_size|none|TRAITS_LENGTH,CONVERT,S_SIZE,S_MAX,S_CSTR_GET,S_CONCAT_PTRS
S_PLUS_RL|xstring|3213-3217|basic_string<_Elem, _Traits, _Alloc>&& _Left, const basic_string|none|_Left;_Right|none|none|S_APPEND_STRING,MOVE,S_MOVE
S_PLUS_RR|xstring|3219-3229|basic_string<_Elem, _Traits, _Alloc>&& _Left, basic_string|none|_Left;_Right|none|none|S_CONCAT_RR
S_PLUS_PR|xstring|3232-3236|_In_z_ const _Elem* const _Left, basic_string|P:_Left|_Right|none|none|S_INSERT_CSTR,MOVE,S_MOVE
S_PLUS_RP|xstring|3244-3248|basic_string<_Elem, _Traits, _Alloc>&& _Left, _In_z_|P:_Right|_Left|none|none|S_APPEND_CSTR,MOVE,S_MOVE
S_APPEND_STRING|xstring|1482-1484|append(const basic_string&|none|_Right|none|none|S_PTR,S_APPEND_COUNT
S_APPEND_CSTR|xstring|1533-1535|append(_In_z_|P:_Ptr|receiver|none|none|TRAITS_LENGTH,CONVERT,S_APPEND_COUNT
S_APPEND_COUNT|xstring|1509-1531|append(|P:_Ptr;Z:_Count|receiver|Z:_Old_size;P:_Old_ptr|CH:terminating_zero|S_PTR,TRAITS_MOVE,TRAITS_ASSIGN,S_GROW_APPEND
S_INSERT_CSTR|xstring|1778-1781|insert(const size_type _Off, _In_z_|Z:_Off;P:_Ptr|receiver|none|none|TRAITS_LENGTH,CONVERT,S_INSERT_COUNT
S_INSERT_COUNT|xstring|1729-1776|insert(|Z:_Off,_Count;P:_Ptr|receiver|Z:_Old_size,_Ptr_shifted_after;B:_Check_overlap;P:_Old_ptr,_Insert_at|none|S_CHECK_OFFSET,S_PTR,TRAITS_MOVE,TRAITS_COPY,S_GROW_INSERT
S_GROW_APPEND|xstring|3024-3057|_Reallocate_grow_by(|Z:_Size_increase,_Args_count;P:_Args_ptr;L_APPEND:_Fn|_My_data;_Al|Z:_Old_size,_New_size,_Old_capacity,_New_capacity;P:_New_ptr,_Raw_new,_Old_ptr|none|S_MAX,S_GROWTH,S_ALLOC_CAP,FA_ORPHAN,UNFANCY,L_APPEND_CALL,S_DEALLOC_CAP,CONSTRUCT_IN_PLACE
S_GROW_INSERT|xstring|3024-3057|_Reallocate_grow_by(|Z:_Size_increase,_Args_off,_Args_count;P:_Args_ptr;L_INSERT:_Fn|_My_data;_Al|Z:_Old_size,_New_size,_Old_capacity,_New_capacity;P:_New_ptr,_Raw_new,_Old_ptr|none|S_MAX,S_GROWTH,S_ALLOC_CAP,FA_ORPHAN,UNFANCY,L_INSERT_CALL,S_DEALLOC_CAP,CONSTRUCT_IN_PLACE
L_APPEND_CALL|xstring|1524-1529|[](_Elem* const _New_ptr|P:_New_ptr,_Old_ptr,_Ptr;Z:_Old_size,_Count|closure receiver|none|CH:terminating_zero|TRAITS_COPY,TRAITS_ASSIGN
L_INSERT_CALL|xstring|1769-1774|[](_Elem* const _New_ptr|P:_New_ptr,_Old_ptr,_Ptr;Z:_Old_size,_Off,_Count|closure receiver|none|none|TRAITS_COPY
S_MAX|xstring|2377-2384|max_size()|none|receiver|Z:_Alloc_max,_Storage_max|Z:max_buffer_size,min_limit,min_storage_minus1|ALLOC_MAX,MAX_LIMIT,MINMAX
S_GROWTH|xstring|2990-2992|_Calculate_growth(const size_type _Requested)|Z:_Requested|receiver|none|none|S_MAX,S_GROWTH_STATIC
S_GROWTH_STATIC|xstring|2976-2988|_Calculate_growth(|Z:_Requested,_Old,_Max|none|Z:_Masked|Z:geometric_growth_operand|MINMAX
S_ALLOC_CAP|xstring|823-847|_Allocate_for_capacity(|none|_Al;_Capacity|P:_Fancy_ptr|none|ALLOC_AT_LEAST
S_DEALLOC_CAP|xstring|849-853|_Deallocate_for_capacity(|P:_Old_ptr;Z:_Capacity|_Al|none|none|ALLOC_DEALLOC
S_DTOR|xstring|1382-1394|~basic_string()|none|receiver|none|none|S_TIDY
S_TIDY|xstring|3082-3097|_Tidy_deallocate()|none|_My_data;_Al|none|CH:terminating_zero|FA_ORPHAN,S_LARGE,S_DEALLOC_CAP,S_SWITCH,TRAITS_ASSIGN
S_PTR|xstring|434-450|_Myptr()|none|receiver|P:_Result|none|S_LARGE,UNFANCY
S_LARGE|xstring|452-454|_Large_mode_engaged()|none|receiver|none|none|none
S_SIZE|xstring|2372-2375|size()|none|receiver|none|none|none
S_CSTR_GET|xstring|2355-2361|c_str()|none|receiver|none|none|S_PTR
S_CHECK_OFFSET|xstring|470-476|_Check_offset(|Z:_Off|receiver|none|none|none
S_ACTIVATE|xstring|456-468|_Activate_SSO_buffer()|none|receiver|none|none|none
S_SWITCH|xstring|509-521|_Switch_to_buf()|none|receiver|none|none|DESTROY_IN_PLACE
S_BEGINEND|xstring|2263-2284|begin()|none|receiver|none|none|S_PTR,S_ITER_CTOR,ADDRESS
S_ITER_CTOR|xstring|42-44|_String_const_iterator(_Mystr*|P:_Parg,_Pstring|receiver|none|none|FA_ADOPT
S_ITER_DEREF|xstring|47-60|operator*()|none|receiver|none|none|UNFANCY
S_ITER_INC|xstring|65-78|operator++()|none|receiver|none|none|none
S_ITER_EQ|xstring|173-177|operator==|none|_Right|none|none|none
TRAITS_LENGTH|__msvc_string_view.hpp|443-461|size_t length(|P:_First|none|none|none|PRIMITIVE
TRAITS_COPY|__msvc_string_view.hpp|104-126|_Elem* copy(|P:_First1,_First2;Z:_Count|none|none|none|PRIMITIVE
TRAITS_MOVE|__msvc_string_view.hpp|135-187|_Elem* move(|P:_First1,_First2;Z:_Count|none|none|none|PRIMITIVE
TRAITS_ASSIGN|__msvc_string_view.hpp|496-503|void assign(|none|_Left;_Right|none|none|none
UB4|xutility|7462-7481|_FwdIt upper_bound(|ITER:_First,_Last;PRED:_Pred|_Val|P:_UFirst,_UMid;DIFF:_Count,_Count2|none|VERIFY,UNWRAP,DISTANCE,NEXT,PRED_CALL,NEXT_ITER,SEEK
UB3|xutility|7485-7488|_FwdIt upper_bound(|ITER:_First,_Last|_Val|none|none|UB4
PRED_CALL|type_traits|2385-2394|struct less<void>|none|_Left;_Right;receiver|none|none|none
EQ_PRED_CALL|xutility|694-706|struct equal_to<void>|none|_Left;_Right;receiver|none|none|none
VERIFY|xutility|1352-1363|_Adl_verify_range(|none|_First;_Last|none|none|none
UNWRAP|xutility|1378-1389|_Get_unwrapped(|none|_It|none|none|V_ITER_UNWRAP
UNWRAP_N|xutility|1439-1467|_Get_unwrapped_n(|DIFF:_Off|_It|none|none|none
RANGE_UNWRAP|xutility|2722-2745|_Unwrap_iter(|none|_It_or_Se|none|none|none
DISTANCE|xutility|1660-1673|distance(|P:_First,_Last|none|none|none|none
NEXT|xutility|1682-1688|next(|P:_First;DIFF:_Off|none|none|none|ADVANCE
ADVANCE|xutility|1632-1657|advance(|DIFF:_Off|_Where|none|none|none
NEXT_ITER|xutility|1677-1679|_Next_iter(|P:_First|none|none|none|none
SEEK|xutility|1477-1483|_Seek_wrapped(|none|_It;_UIt|none|none|V_ITER_UNWRAP
TO_ADDRESS|xutility|4615-4619|_To_address(|none|_Val|none|none|RAW_ADDRESS
RAW_ADDRESS|xutility|540-543|to_address(_Ty*|P:_Val|none|none|none|none
COPY_MEMMOVE|xutility|4762-4771|_Copy_memmove(|P:_First,_Last,_Dest|none|P:_First_ptr,_Last_ptr,_First_ch,_Last_ch;Z:_Object_count,_Byte_count|none|TO_ADDRESS,COPY_TAIL
COPY_MEMMOVE_N|xutility|4774-4779|_Copy_memmove_n(|P:_First,_Dest;Z:_Object_count|none|P:_First_ptr,_First_ch;Z:_Byte_count|none|TO_ADDRESS,COPY_TAIL
COPY_TAIL|xutility|4746-4759|_Copy_memmove_tail(|P:_First_ch,_Dest;Z:_Byte_count,_Object_count|none|P:_Dest_ptr,_Dest_ch|none|TO_ADDRESS,PRIMITIVE
FILL_ZERO|xutility|5269-5271|_Fill_zero_memset(|P:_Dest;Z:_Count|none|none|none|TO_ADDRESS,PRIMITIVE
BITS_ZERO|xutility|5275-5283|_Is_all_bits_zero(|none|_Val|D:_Zero|none|PRIMITIVE
IDL_DISTANCE|xutility|1550-1559|_Idl_distance(|none|_First;_Last|none|none|none
EQUAL3|xutility|5539-5542|bool equal(|P:_First1,_Last1,_First2|none|none|none|EQUAL4
EQUAL4|xutility|5504-5530|bool equal(|P:_First1,_Last1,_First2;EQ:_Pred|none|P:_UFirst1,_ULast1,_UFirst2,_First1_ch;DIFF:_Size|none|VERIFY,UNWRAP,UNWRAP_N,IDL_DISTANCE,TO_ADDRESS,PRIMITIVE
ARRAY_SUBSCRIPT|array|529-546|operator[]|Z:_Pos|receiver|none|none|none
ARRAY_DATA|array|562-568|data()|none|receiver|none|none|none
ARRAY_SIZE|array|501-503|size()|none|receiver|none|none|none
ARRAY_EQUAL|array|777-779|bool operator==|none|_Left;_Right|none|none|ARRAY_DATA,EQUAL3
PAIR_DEFAULT|utility|250-256|pair()|none|receiver|none|none|none
PAIR_FORWARD|utility|268-278|pair(_Other1&&|none|_Val1;_Val2|none|none|FORWARD
PAIR_ASSIGN|utility|363-369|operator=(_Identity_t<_Myself&&>|none|_Right|none|none|FORWARD
I_CTOR|initializer_list|33-34|initializer_list(const _Elem*|P:_First_arg,_Last_arg|receiver|none|none|none
I_BEGIN|initializer_list|36-38|begin()|none|receiver|none|none|none
I_END|initializer_list|40-42|end()|none|receiver|none|none|none
I_SIZE|initializer_list|44-46|size()|none|receiver|none|none|none
MOVE|type_traits|1512-1514|move(_Ty&&|none|_Arg|none|none|none
FORWARD|type_traits|1501-1509|forward(|none|_Arg|none|none|none
ADDRESS|xutility|523-531|addressof(|none|referent|none|none|none
EXCHANGE|utility|770-776|exchange(|none|_Val;_New_val|P:_Old_val|none|none
MINMAX|utility|71-103|(max) (|none|_Left;_Right|none|none|none
MAX_LIMIT|utility|863-872|_Max_limit()|none|none|none|none|none
BIT_CAST|bit|31-33|bit_cast(|none|_Val|none|none|none
LIMITS_GET|limits|629-641|class numeric_limits<long long>|none|none|none|none|none
ISFINITE|corecrt_math.h|435-438|bool isfinite(|D:_X|none|none|none|FPCLASSIFY
FPCLASSIFY|corecrt_math.h|321-327|int fpclassify(|D:_X|none|none|none|PRIMITIVE
EXCEPTION_DEFAULT|vcruntime_exception.h|51-54|exception()|none|receiver|none|none|none
EXCEPTION_MESSAGE|vcruntime_exception.h|56-62|exception(char const* const _Message)|P:_Message|receiver|EXDATA:_InitData|none|PRIMITIVE
EXCEPTION_COPY|vcruntime_exception.h|71-75|exception(exception const&|none|_Other;receiver|none|none|PRIMITIVE
EXCEPTION_DTOR|vcruntime_exception.h|88-91|~exception()|none|receiver|none|none|PRIMITIVE
EXCEPTION_WHAT|vcruntime_exception.h|93-96|what()|none|receiver|none|none|none
RUNTIME_ERROR|stdexcept|100-106|runtime_error(const char*|P:_Message|receiver|none|none|EXCEPTION_MESSAGE
LOGIC_ERROR|stdexcept|20-26|logic_error(const char*|P:_Message|receiver|none|none|EXCEPTION_MESSAGE
ALLOC_EQUAL|xmemory|1060-1064|operator==|none|left_allocator;right_allocator|none|none|none
PRIMITIVE|CPP|5-17|#include <bit>|none|none|none|none|none
'''

def objects(s, role):
    if s=='none': return []
    out=[]
    for group in s.split(';'):
        ty,names=group.split(':',1)
        out += [{'name':n,'type':ty,'role':role,'count':1} for n in names.split(',')]
    return out

rows=[]
for raw in SPEC.splitlines():
    if not raw.strip(): continue
    f=raw.split('|')
    if len(f)!=9: raise ValueError(('SPEC fields',raw,len(f)))
    ident,src,span,decl,params,refs,locals_,temps,calls=f
    lo,hi=map(int,span.split('-'))
    snippet='\n'.join(text[src].splitlines()[lo-1:hi])+'\n'
    rows.append({'id':ident,'source':src,'lines':[lo,hi],'declaration_anchor':decl,'source_excerpt':snippet,'source_excerpt_sha256':hashlib.sha256(snippet.encode()).hexdigest().upper(),'value_parameters':objects(params,'by-value formal'),'reference_bindings':[] if refs=='none' else refs.split(';'),'locals':objects(locals_,'automatic declaration'),'materializations':objects(temps,'actual materialization'),'children':[] if calls=='none' else calls.split(','),'return_rule':'Reference returns alias their referent. Scalar returns enter an actual caller destination only where materialized. Class direct-prvalue result uses the actual destination. Parameter/local return alternatives retain the source object; optional NRVO is not assumed.'})

for r in rows:
    r['scope']='Selected runtime branch; declarations belonging only to excluded branches are not counted. Local declaration union is an explicit safe source component, not a physical frame or exact simultaneous-byte claim.'
    r['exception_rule']='Allocation/ordinary failure retains entered source-owned objects through actual unwind. No heap-free/return/destructor/GC or process-exit storage refund.'

profile={
 'selected':'MSVC14.44.35207,194435228,x64,C++20,/MD,/EHsc,/O2,/fp:strict,/permissive-,_ITERATOR_DEBUG_LEVEL=0; no _DEBUG, sanitizer or user hardening/internal-check defines in the pinned command',
 'runtime_not_consteval':True,
 'excluded':[
  {'id':'ranges_upper','proof':'xutility7462 calls _Pred directly. No ranges algorithm/_Pass_fn ingress occurs. ranges _Unwrap_iter/_Unwrap_sent DO occur only in vector uninitialized-copy wrappers; those reference-only wrappers are separately present.'},
  {'id':'debug_proxy','proof':'xmemory1502–1506 selects _Fake_allocator and _Fake_proxy_ptr_impl. _Container_base0/_Iterator_base0 contain no dynamic proxy. Their actual standalone fake objects are retained, not sized as zero.'},
  {'id':'asan_and_tombstone','proof':'__msvc_sanitizer_annotate_container.hpp selects annotations only under sanitizer/_ANNOTATE defines absent from exact command; yvals.h275–277 defaults tombstones0. No _DEBUG or _ENABLE_STL_INTERNAL_CHECK was added.'},
  {'id':'nondefault_allocator','proof':'All vector/string instantiations use std::allocator<T>; xmemory supplies is_always_equal and propagation=true. Unequal-allocator move branches and rebind proxy allocations are not selected.'},
  {'id':'iterator_string_constructor','proof':'All String construction ingress is pointer, pointer/count, selected concat or move. xstring798 iterator overload/_Construct_from_iter and its _Tidy_deallocate_guard at961 are not called.'},
  {'id':'leave_proxy_unbound','proof':'All reached fake proxy constructors receive the existing container reference (xmemory1450), not _Leave_proxy_unbound. Do not request that unrelated tag layout.'},
  {'id':'vector_reuse_or_nonthow_backout','proof':'Numeric prefix starts as default empty vector; CompiledTempoMap::build resets out to fresh empty vectors before assign. All3/4 counts exceed old capacity0. POD Change/TempoChange/double select memmove or known+0.0 zero fill at runtime; _Uninitialized_backout_al fallback is not reached. _Tidy_guard<vector<qc::Change>> at2115 DOES materialize.'},
  {'id':'manual_alignment','proof':'Maximum vector request4*24=96; conservative String count<=641, old capacity<=527, one grow capacity<=max(641|15,527+527//2)=790, requested791. All are below4096 and align<=default alignment16; xmemory manual/aligned allocation and adjustment branches not reached.'},
  {'id':'length_and_offset_errors','proof':'All vector counts3/4, strings<=641 and insert offset0 meet selected max_size/offset arithmetic. Those SDK length/offset/overflow throw branches are not reached by admitted owned source values; ordinary allocation failure remains admitted.'},
  {'id':'constant_only','proof':'if constexpr, requires, sizeof/alignof, trait/_Pocma selectors and unevaluated noexcept create no runtime object by themselves. _Is_all_bits_zero::_Zero IS retained because its address is passed to memcmp.'}
 ]}

# Preserve precise per-expression ownership instead of treating one source row as
# an arbitrary allocator allowance. No loop visit multiplies simultaneous locals.
joins=[
 {'id':'vector_init_list','chain':['V_ILIST','V_CONSTRUCT','UNINIT_COPY','COPY_MEMMOVE','COPY_TAIL'],'persistent':'actual vector destination; <=4 Change initializer backing; initializer-list formal16; default allocator temp; constructor tag only during meminitializer; _Construct_n fake allocator/proxy and guard persist through copy','materializations':'two initializer_list.begin/end returned raw pointers bind _Construct_n forwarding refs; two8-byte pointer objects. No extra vector return destination beyond CachedPort.changes.','maximum_instances':{'V_CONSTRUCT':1,'FP':1,'FA':1,'TG':1,'ALLOC_CHANGE':1,'initializer_backing_Change':4,'forwarded_pointer':2}},
 {'id':'vector_copy','chain':['V_COPY','V_CONSTRUCT','UNINIT_COPY','COPY_MEMMOVE','COPY_TAIL'],'persistent':'CachedPort.changes24 + NumericAClock changes by-value formal24 + destination member24 inside containing CachedPort/base. The fresh copy owns its independent backing.','materializations':'allocator-select returned allocator is one temp during compressed-pair initializer; _Construct_n forwarding arguments are existing pointer members, not new materialized pointers.','maximum_instances':{'V_CONSTRUCT':1,'FP':1,'FA':1,'TG':1,'ALLOC_CHANGE':1,'independent_vector_headers':3}},
 {'id':'vector_move','chain':['V_MOVE','C_ONE','EXCHANGE','V_VAL_CTOR'],'persistent':'source and destination vector headers; no backing clone merely from move. Three exchange nullptr arguments and returned pointer destinations persist through compressed-pair full expression.','materializations':'3 nullptr_t temporaries (not assumed pointer-sized),3 pointer-return materializations24, and _Vector_val by-value First/Last/End24. exchange::_Old_val one pointer8 during one sibling exchange; siblings execute separately.','maximum_instances':{'TAG_ONE':1,'NUL':3,'forwarded_pointer':3,'Vector_val_pointer_formal':3,'exchange_Old_val':1}},
 {'id':'vector_assign_fresh','chain':['V_ASSIGN_RANGE','V_ASSIGN_COUNTED','V_CLEAR','V_BUY_RAW','ALLOC_AT_LEAST','ALLOC_ALLOCATE','ALLOC_BYTES','DEFAULT_NEW'],'persistent':'fresh Production destination/count3or4. Pointer range parameters, unwrap destinations and length/count scalars are separate; no referent copies. Allocation branch completes before copy.','materializations':'Production.startFrame and qc prefix fill each binds one double+0.0 temp to const reference; known-bit-zero check local _Zero double8 remains an actual object.','maximum_instances':{'allocated_elements':4,'D_zero_fill':1,'BITS_ZERO_Zero':1,'FP':0,'TG':0}},
 {'id':'string_construct','chain':['S_CSTR','S_CONSTRUCT','S_ALLOC_CAP','ALLOC_AT_LEAST','ALLOC_ALLOCATE','ALLOC_BYTES','DEFAULT_NEW'],'persistent':'actual String32; _Construct source pointer/count; fake allocator+proxy; New_capacity/New_ptr only large branch. Small and large trait calls are sequential.','materializations':'constructor tag is one value parameter to compressed_pair. Per terminator assign _Elem() materializes one char bound to const char&; no hidden String header.','maximum_instances':{'FA':1,'FP':1,'TAG_ZERO':1,'terminator_char':1}},
 {'id':'string_concat_lvalue','chain':['S_PLUS_LP','S_CONCAT_PTRS','S_ALLOC_CAP','ALLOC_AT_LEAST','ALLOC_ALLOCATE','ALLOC_BYTES','DEFAULT_NEW'],'persistent':'lvalue operands referenced; returned String destination32 is one actual object; concat-tag formal, allocator-copy result, compressed-pair tag, fake allocator/proxy are distinct when active.','materializations':'Each pointer/size formal is a value object; source strings and contained allocators are not copied as referents.','maximum_instances':{'TAG_CONCAT':1,'ALLOC_CHAR':1,'TAG_ONE':1,'FA':1,'FP':1}},
 {'id':'string_grow_append','chain':['S_PLUS_RP','S_APPEND_CSTR','S_APPEND_COUNT','S_GROW_APPEND','L_APPEND_CALL','TRAITS_COPY'],'persistent':'source String and actual move-return destination are distinct. Growth callback _Fn is one by-value object; its operator adds3 pointer and2 size formals while it remains alive. Allocation and callback phases are sequential.','materializations':'_Args pointer/count16 are separate by-value copies. The noncapturing append closure initializes Fn directly; no extra caller lambda copy.','maximum_instances':{'L_APPEND':1,'lambda_pointer_formals':3,'lambda_size_formals':2,'copied_growth_arg_pointer':1,'copied_growth_arg_size':1,'terminator_char':1}},
 {'id':'string_grow_insert','chain':['S_PLUS_PR','S_INSERT_CSTR','S_INSERT_COUNT','S_GROW_INSERT','L_INSERT_CALL','TRAITS_COPY'],'persistent':'source String and actual move-return destination are distinct. Fn remains alive while operator adds3 pointer and3 size formals.','materializations':'_Args Off/count16 plus pointer8 are separate by-value copies; noncapturing insert closure initializes Fn directly.','maximum_instances':{'L_INSERT':1,'lambda_pointer_formals':3,'lambda_size_formals':3,'copied_growth_arg_pointer':1,'copied_growth_arg_size':2}},
 {'id':'string_take','chain':['S_MOVE','S_TAKE','TRAITS_ASSIGN'],'persistent':'both headers32 coexist. Selected _Can_memcpy_val true for raw pointer/default allocator String under no annotation; two raw pointer locals16.','materializations':'one char zero temp for terminator reference; source/destination subobjects stay inside measured headers.','maximum_instances':{'source_and_destination_String':2,'P_take_locals':2,'terminator_char':1}},
 {'id':'upper_tick4','chain':['UB4','NEXT','ADVANCE'],'persistent':'2 input const-iterator formals16 + predicate closure + raw UFirst8 + Count8; each iteration adds Count2/UMid16. next adds First/Off16 and advance Off8.','materializations':'next returned pointer initializes UMid directly. Alternative Next_iter adds one pointer value formal8 and assigns scalar result; those helper calls are sequential. Return First is a parameter (NRVO forbidden), so actual returned iterator destination8 is separate while First still exists.','maximum_instances':{'ITER_formals':2,'own_stateless_predicate':1,'raw_search_locals':2,'difference_search_locals':2,'NEXT_pointer_formal':1,'NEXT_diff_formal':1,'ADVANCE_diff_formal':1,'returned_iterator':1}},
 {'id':'upper_prefix3','chain':['UB3','UB4','NEXT','ADVANCE'],'persistent':'outer3arg First/Last16 coexist with inner4arg copies16 and one less<void> predicate. No extra less temporary in addition to its direct value formal.','materializations':'inner First return initializes the actual outer caller destination through direct prvalue forwarding; no third return iterator invented. A changes.begin()/prefix.begin() sibling subtraction operand may already be initialized and persists through search, so retain one additional8-byte iterator.','maximum_instances':{'ITER_formals':4,'LESS':1,'returned_iterator':1,'sibling_begin_iterator':1}},
 {'id':'witness_array_equality','chain':['ARRAY_EQUAL','EQUAL3','EQUAL4','UNWRAP_N'],'persistent':'the two array48 referents remain existing objects. Three pointer formals24 in equal3 + three24 in equal4 + equal_to<void> value predicate.','materializations':'unwrapped three pointers24, First1_ch8 and Size8; Idl_distance uses references only; Unwrap_n has one actual difference8 formal. Runtime safe-uint64 memcmp branch selected; predicate body not called.','maximum_instances':{'EQ':1,'pointer_formals_equal':6,'pointer_locals_equal':4,'diff_local_equal':1}},
 {'id':'exception_message','chain':['RUNTIME_ERROR','EXCEPTION_MESSAGE'],'persistent':'runtime_error/logic_error24 actual exception destination, by-value Message pointer at each constructor layer, and one __std_exception_data aggregate; __std_exception_copy imported primitive remains existing runtime boundary.','materializations':'fixed literal message clone request is length+1 in separate NativeHeap ledger. Source-derived Rejection uses default exception base, no _InitData; payload80 contains base and witness.','maximum_instances':{'EXDATA':1,'exception_Message_formals':2,'runtime_error_payload':1}},
 {'id':'exception_control_nested','chain':['ARRAY_EQUAL','EQUAL3','EQUAL4'],'persistent':'caught original Rejection80 can coexist with a new wrong-control/cleanup Rejection80 plus actual gate and Rejection constructor witness48 formals; caught references do not copy exception payloads.','materializations':'throw prvalue constructs actual exception destination directly. All unwind-held source context stays retained until actual scope ends; no generic EH heap or physical-frame fact is asserted.','maximum_instances':{'Rejection_payload':2,'EXDATA':0}},
]

new_closures=[
 {'id':'SDK_append_grow','source':'xstring','lines':[1524,1529],'captures':[],'source_type':'exact noncapturing lambda at xstring1524','destination':'_Reallocate_grow_by _Fn by-value parameter','object_instances':1,'additional_copy_instances':0,'sizeof':None,'alignof':None,'operator_value_formals':{'P':3,'Z':2},'context':'S_APPEND_COUNT -> S_GROW_APPEND -> L_APPEND_CALL; cannot coexist with SDK_insert_grow'},
 {'id':'SDK_insert_grow','source':'xstring','lines':[1769,1774],'captures':[],'source_type':'exact noncapturing lambda at xstring1769','destination':'_Reallocate_grow_by _Fn by-value parameter','object_instances':1,'additional_copy_instances':0,'sizeof':None,'alignof':None,'operator_value_formals':{'P':3,'Z':3},'context':'S_INSERT_COUNT -> S_GROW_INSERT -> L_INSERT_CALL; cannot coexist with SDK_append_grow'}]

types={'P':{'size':8,'basis':'194-cell pointer.sizeof; ordinary selected raw pointer parameters'},'Z':{'size':8,'basis':'size_t.sizeof'},'DIFF':{'size':8,'basis':'selected x64 ptrdiff_t is signed64; pointer-difference type source+SDK profile, numeric ABI association remains independent'},'D':{'size':8,'basis':'double.sizeof'},'B':{'size':1,'basis':'bool.sizeof'},'CH':{'size':1,'basis':'char.sizeof'},'IL':{'size':16,'basis':'ChangeInitializerList.sizeof'},'ITER':{'size':8,'basis':'actual selected const iterator sizeof cells'},'NUL':{'size':None,'basis':'actual nullptr_t reference-bound materialization; no pointer-width guess'},'PRED':{'size':None,'basis':'instantiate actual qc/time stateless predicate closure, one at a time'},'LESS':{'size':None,'basis':'std::less<void> by-value predicate at upper_bound4'},'EQ':{'size':None,'basis':'std::equal_to<void> by-value predicate at equal4'},'FA':{'size':None,'basis':'std::_Fake_allocator standalone actual objects'},'FP':{'size':None,'basis':'std::_Fake_proxy_ptr_impl standalone proxy'},'TG':{'size':None,'basis':'std::_Tidy_guard<std::vector<qc::Change>> actual guard'},'TAG_ZERO':{'size':None,'basis':'std::_Zero_then_variadic_args_t by-value compressed-pair formal'},'TAG_ONE':{'size':None,'basis':'std::_One_then_variadic_args_t by-value compressed-pair formal'},'TAG_CONCAT':{'size':None,'basis':'std::_String_constructor_concat_tag by-value concat formal'},'ALLOC_CHANGE':{'size':None,'basis':'std::allocator<qc::Change> default/copy-return temporary'},'ALLOC_CHAR':{'size':None,'basis':'std::allocator<char> copy-return temporary'},'L_APPEND':{'size':None,'basis':'exact SDK_append_grow closure'},'L_INSERT':{'size':None,'basis':'exact SDK_insert_grow closure'},'EXDATA':{'size':None,'basis':'::__std_exception_data automatic _InitData at exception message constructor'}}

for c in closures:
    c['source_incidence']={'maximum_direct_or_named_instances':1,'additional_copies':0,'body_source_objects':({'B':1} if c['id']=='nextBit' else {'u64':1} if c['id']=='get' else {'Tick':1} if c['id'] in ('qc_tick_comparator','time_tick_comparator','time_native_comparator') else {}),'copy_rule':'expect action parameter/upper_bound predicate parameter initialized directly from prvalue; get/nextBit named receiver called by reference; no extra capture referent clones'}

contexts=[]
for old in env['call_contexts']:
    c=dict(old)
    c['source_component_join']='Named exact body objects + actual 24 materialization groups + live own closure destinations/bodies + selected SDK chain under this context. Automatic loop bodies/word visits sequential. Heap backing and static owners remain separately retained.'
    c['sdk_joins']= {
     'entry_quote':['string_construct'], 'common_controls':['vector_init_list','vector_copy','vector_move','vector_assign_fresh','upper_tick4','witness_array_equality','exception_control_nested'],
     'fraction_cache':['exception_message'], 'fixture_build':['vector_init_list','vector_copy','vector_move','vector_assign_fresh','exception_message'],
     'dense_cached':['upper_tick4','exception_message'], 'dense_production':['upper_tick4'], 'metadata_build':['witness_array_equality'],
     'metadata_read':['string_construct','string_concat_lvalue','string_grow_append','string_grow_insert','string_take','exception_control_nested'],
     'metadata_write':['string_construct','string_concat_lvalue','string_grow_append','string_grow_insert','string_take'],
     'fixture_controls':['vector_assign_fresh','upper_tick4','witness_array_equality','exception_control_nested','string_concat_lvalue'],
     'inverse':['upper_prefix3','upper_tick4','string_construct','string_concat_lvalue','string_grow_append','string_take','witness_array_equality'],
     'retirement_failure':['exception_message','exception_control_nested']
    }[c['id']]
    c['maximum_own_closure_objects']=2 if c['id'] in ('dense_cached','dense_production','fixture_controls','inverse') else 1
    c['maximum_SDK_growth_closure_objects']=1 if c['id'] in ('entry_quote','metadata_read','metadata_write','fixture_controls','inverse') else 0
    c['ABI_or_size_upper']=None
    contexts.append(c)

put('selected-sdk-ledger.json',{'schema':'SELECTED_CPP_SDK_OBJECT_INCIDENCE_CANDIDATE_R1','source_closed_candidate':True,'independently_accepted':False,'rows':rows,'type_symbols':types,'profile':profile,'coexistence_algebra':'Each row is a declaration/value-parameter/materialization union within one entered selected body. Add actual enclosing owners plus the active chain; select max of sequential/alternative helper phases. Multiple calls in one full expression retain every actual parameter/materialization destination required by the expression, including sibling begin, forwarded pointer/nullptr and allocator/tag arguments. By-reference aliases add no referent clone. This is a finite symbolic source ownership candidate, not physical frame/RSS or a numeric OTHER upper. The14 explicit joins resolve critical repeated/default/copy/return contexts; no arbitrary multiplier or loop-count simultaneous factor is supplied.','joins':joins,'no_32768_credit':True,'numeric_OTHER_upper':None,'execution':False})
put('closure-incidence.json',{'own_closures':closures,'new_sdk_closures':new_closures,'own_count':len(closures),'SDK_count':len(new_closures),'layout_credit':False,'rule':'Each exact source closure expression is its own type; actual capture fields/layout remain null, never count*8.'})
put('context-joins.json',{'contexts':contexts,'materialization_groups':env['source_materializations'],'critical_joins':joins,'scope':'Complete finite source/context candidate; numeric/type/native association acceptance remains HOLD.'})

# Actual source ingresses. Store all standard-library written spellings in exact
# own source, distinguishing reached from legacy/uninvoked members by named scope.
spelling=[]
for alias in ['CPP','QC','TIME','BOOK']:
    for no,line in enumerate(text[alias].splitlines(),1):
        uses=re.findall(r'std::[A-Za-z_][A-Za-z_0-9]*',line)
        if uses:
            spelling.append({'source':alias,'line':no,'spellings':uses,'source_line':line,'reached_scope_rule':'TIME: only SampleRate/TempoChange/MusicalTime predicates, detail::isFinitePositive/scaledTimeRatio/checkedNativeFrame/appendTempoSegmentFrames and CompiledTempoMap build/frameForTick/frameForMusicalTime/musicalTimeForFrame/default-copy-move-destruction. QC: require/fixtureMap/scaled/checkFrame/fractionBounds/NumericAClock constructor/append/segmentFor/frame/inverse. CPP bytesGate and QC NumericAClock::cut not invoked. All remaining legacy TIME free conversion/meter/UI methods excluded.'})
put('written-sdk-ingresses.json',{'rows':spelling,'selection':'Exact source text inventory, not an AST/compiler execution. Unreached declarations are explicitly filtered by scope rules; type names alone create no runtime object.'})

needed=[{'id':key,'sizeof':value['size'],'reason':value['basis'],'needed_new_fact':value['size'] is None} for key,value in types.items()]
needed += [{'id':'TempoMapView','sizeof':None,'alignof':None,'reason':'Actual by-value Time::build formal; type missing from194 cells','needed_new_fact':True}]
needed += [{'id':'OWN_CLOSURE_'+c['id'],'sizeof':None,'alignof':None,'reason':'Exact source expression/capture/body correspondence in closure-incidence.json','needed_new_fact':True} for c in closures]
put('required-type-facts.json',{'facts':needed,'existing_layout_cells_used':194,'new_measurements_performed':0,'minimality':'No fake size for empty types. No _Leave_proxy_unbound, iterator string _Tidy_deallocate_guard, ASan/debug proxy or unequal-allocator guard size is requested because those branches are not reached. Distinct specializations are not equated without an accepted ABI theorem. Further general runtime bookkeeping is outside this source-object scope and unqualified, not zero.'})

messages=[]
for alias in ['QC','BOOK']:
    for no,line in enumerate(text[alias].splitlines(),1):
        for literal in re.findall(r'"([A-Z][A-Z0-9_]+)"',line):
            messages.append({'source':alias,'line':no,'literal':literal,'requested_copy_bytes':len(literal)+1})
put('message-copy-requests.json',{'messages':messages,'maximum_literal_plus_NUL':max(x['requested_copy_bytes'] for x in messages),'automatic_InitData_instances':1,'scope':'Visible message constructor request only. Does not qualify imported __std_exception_copy, EH allocator overhead, refcounts or runtime bookkeeping. Rejection reason pointers reference existing literals without a message clone.'})

inputs=[]
for name in ['source-pins.json','first-green-freeze.json','argument.md','envelope.json','own-named-objects.json','closure-contexts.json']:
    p=OLD/name; inputs.append({'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)})
for alias,p in sources.items():
    inputs.append({'alias':alias,'path':p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)})
for name in ['build-ci/20261106-native-object-layout-purpose-source-r1/execution-r1/layout-values.json','build-ci/20261105-clock-oracle-separate-research-contract/contract.md','build-ci/20261106-native-object-layout-purpose-source-r1/profile.json']:
    p=ROOT/name;inputs.append({'path':name,'bytes':p.stat().st_size,'sha256':sha(p)})
put('source-pins.json',{'schema':'NATIVE_SDK_SOURCE_CONTEXT_PINS_R1','inputs':inputs,'source_only':True,'execution':False,'scope':'Exact frozen envelope and R13 native target/selected source profile. No final compiler/header/binary association or wholefit follows.'})
put('authoring.json',{'source_rows':len(rows),'closure_expressions':len(closures)+len(new_closures),'critical_joins':len(joins),'contexts':len(contexts),'missing_type_symbols':sum(x['size'] is None for x in types.values())+1+len(closures),'source_ingress_lines':len(spelling),'all_target_execution':False,'status':'Complete source-incidence/context candidate, awaiting own check and independent review; numeric/type/native/wholefit remains HOLD.'})
print(json.dumps({'rows':len(rows),'contexts':len(contexts),'critical_joins':len(joins),'source_only':True}))
