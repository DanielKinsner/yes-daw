# Research PowerShell signed literal: one value read

The contiguous PowerShell numeric literal `-1` is one signed-number token and
costs one literal R. It has no separate reached unary-minus O. This is a lexical
source distinction, not an optimizer discount or a rule for C++ negative values.

Selected primary source: Parser.cs UnaryExpressionRule at7029 sets
AllowSignedNumbers=true, including resynchronization of a previously read Minus
token. tokenizer.cs4745-4769 passes the sign into ScanNumber when the immediately
following character is a digit or dot. PrimaryExpression creates a
ConstantExpressionAst from the NumberToken at Parser.cs7258.

A separated `- 1` or `-$value` reaches a genuine unary operator and retains its
operator O as well as the operand read. Apply the signed-number distinction
consistently to actual written PowerShell EOF/signed-literal sites; do not
blindly transfer an old worksheet's extra action.

Pinned files under the existing selected PowerShell primary inventory:

- parser/Parser.cs:4B828414E20F48094BC161234DE3109FDA7A2AC63EDA7BF93F03CF15498AA992.
- parser/tokenizer.cs:48AEAB2FD2A803B610AC6AA6EC2E8A2DEB08D42297409780BA6CD127F6B25E42.

Only file/source text was read. No target parser, AST, helper or compiler ran.
Original conventions and frozen prices remain unchanged; R2 still requires a
fresh whole review, with no Data, resource fit, native binding or execution GO.
