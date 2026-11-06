"""Writes an UNRUN source successor; never executes/compiles the C# reader."""
from pathlib import Path
import difflib
import hashlib
import json

ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/'build-ci/20261105-managed-file-type-inventory-source-r1'
OUT=Path(__file__).resolve().parents[1]
if hashlib.sha256((BASE/'reader.cs').read_bytes()).hexdigest().upper() != 'E9B1CC2AC4D5B1B3C8D1DE955A30A938E95EADB1FA92C8757350DB07BADACC74':
    raise ValueError('frozen revision1 reader bytes changed')
source=(BASE/'reader.cs').read_text(encoding='utf-8')
original=source
def replace(old,new):
    global source
    if source.count(old)!=1:raise ValueError('source delta anchor not unique')
    source=source.replace(old,new)

replace('    private sealed class InventoryFailure : Exception', '''    private static readonly string[] OptionalCoreTypes =
    {
        "System.Collections.Generic.List`1+Enumerator",
        "System.ArrayEnumerator",
        "System.SZGenericArrayEnumerator`1"
    };
    private sealed record NestedDiscovery(string File, string Parent, string[] Names, string Selected);
    private sealed record OptionalPresence(string File, string Type, bool Present);

    private sealed class InventoryFailure : Exception''')
replace('            json.WriteString("schema", "FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V1");',
        '            json.WriteString("schema", "FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V2");')
replace('            json.WriteNumber("requested_types", 21);', '''            json.WriteNumber("required_named_types", 21);
            json.WriteNumber("optional_named_types", 3);
            json.WriteNumber("required_generated_type_selections", 1);''')
replace('            int count = 0;\n            foreach (Input input in Inputs)', '''            int count = 0;
            int requiredCount = 0;
            int generatedCount = 0;
            var discoveries = new List<NestedDiscovery>();
            var optionalPresence = new List<OptionalPresence>();
            foreach (Input input in Inputs)''')
replace('                // This walks only TypeDef name metadata, not methods, IL, attributes or executable code.', '''                bool core = String.Equals(input.File, "System.Private.CoreLib.dll", StringComparison.Ordinal);
                if (core) foreach (string type in OptionalCoreTypes) wanted.Add(type, default);
                // This walks only TypeDef name metadata, not methods, IL, attributes or executable code.''')
replace('                    count++;\n                }\n            }\n            if (count != 21)', '''                    count++;
                    requiredCount++;
                }
                if (String.Equals(input.File, "Newtonsoft.Json.dll", StringComparison.Ordinal))
                {
                    const string parentName = "Newtonsoft.Json.Linq.JObject";
                    const string prefix = parentName + "+<GetEnumerator>d__";
                    var names = new List<string>();
                    TypeDefinitionHandle selected = default;
                    string selectedName = "";
                    foreach (TypeDefinitionHandle nested in reader.GetTypeDefinition(wanted[parentName]).GetNestedTypes())
                    {
                        string name = provider.DefinitionName(nested);
                        names.Add(name);
                        if (!name.StartsWith(prefix, StringComparison.Ordinal)) continue;
                        bool numericSuffix = name.Length > prefix.Length;
                        for (int at = prefix.Length; at < name.Length; at++)
                            if (name[at] < '0' || name[at] > '9') numericSuffix = false;
                        if (!numericSuffix) continue;
                        if (!selected.IsNil) throw new InventoryFailure("AMBIGUOUS_ENUMERATOR_NESTED_NAME");
                        selected = nested;
                        selectedName = name;
                    }
                    if (selected.IsNil) throw new InventoryFailure("ENUMERATOR_NESTED_NAME_MISSING");
                    discoveries.Add(new NestedDiscovery(input.File, parentName, names.ToArray(), selectedName));
                    WriteType(json, input, reader, provider, selected, selectedName);
                    count++;
                    generatedCount++;
                }
                if (core)
                {
                    foreach (string type in OptionalCoreTypes)
                    {
                        TypeDefinitionHandle handle = wanted[type];
                        bool present = !handle.IsNil;
                        optionalPresence.Add(new OptionalPresence(input.File, type, present));
                        if (!present) continue;
                        WriteType(json, input, reader, provider, handle, type);
                        count++;
                    }
                }
            }
            if (requiredCount != 21 || generatedCount != 1 || discoveries.Count != 1 || optionalPresence.Count != 3
                || count < 22 || count > 25)''')
replace('            json.WriteNumber("reported_types", count);\n            json.WriteEndObject();', '''            json.WriteNumber("reported_types", count);
            json.WriteStartArray("nested_type_name_discoveries");
            foreach (NestedDiscovery discovery in discoveries)
            {
                json.WriteStartObject();
                json.WriteString("file", discovery.File);
                json.WriteString("parent", discovery.Parent);
                json.WriteStartArray("declared_nested_type_names");
                foreach (string name in discovery.Names) json.WriteStringValue(name);
                json.WriteEndArray();
                json.WriteString("selected_generated_type", discovery.Selected);
                json.WriteEndObject();
            }
            json.WriteEndArray();
            json.WriteStartArray("optional_type_presence");
            foreach (OptionalPresence presence in optionalPresence)
            {
                json.WriteStartObject();
                json.WriteString("file", presence.File);
                json.WriteString("qualified_name", presence.Type);
                json.WriteBoolean("present", presence.Present);
                json.WriteEndObject();
            }
            json.WriteEndArray();
            json.WriteEndObject();''')
(OUT/'reader.cs').write_text(source,encoding='utf-8',newline='\n')
readme='''# Fixed managed type inventory — revision2 source only

**UNCOMPILED / UNRUN. Independent whole source review is required before root
compiles or invokes this separate metadata tool.** It does not run the R11 runner,
an allocation probe, a target constructor, or target executable code. It grants
no object-size/offset/live/reclaim/32MiB fit, source price, binding or stage GO.
Frozen revision1 and its actual executions remain unchanged.

Revision2 retains all four exact input file lengths/hashes and all21 required
named types from revision1. It adds only this requested metadata closure:

- List the names of the types directly declared inside Newtonsoft.Json.Linq.JObject.
- Select exactly one direct child whose complete name begins with
  `Newtonsoft.Json.Linq.JObject+<GetEnumerator>d__` and has a nonempty ASCII decimal
  suffix. Missing or ambiguous selection fails; no generated numeric suffix is
  guessed. Report that selected type with the existing declared-field/base/signature
  inventory. The names of other children are listed without their fields or code.
- Look for CoreLib ``System.Collections.Generic.List`1+Enumerator``,
  `System.ArrayEnumerator`, and ``System.SZGenericArrayEnumerator`1``.
  Report each one's presence; inventory its fields/base/signatures only when
  present. Absence is data, never a fabricated field row or successful presence.

The JSON schema is V2. Success requires all21 original required definitions,
one selected generated definition, and three optional presence records. The
reported field-inventoried type count is22 through25. The nested-name list and
optional presence rows are measurement-generated file metadata; no manual PASS
rows are added. Output is buffered and published once, after the whole inventory
has passed its source count/selection checks. A fixed bounded stderr category and
reason code with ordinary exit1 represents failure; no exception message, path,
stack, environment, field value or partial inventory is published.

The unchanged reader pins each read-only DLL handle by length and SHA256 before
rewinding and parsing its public PE/CLR file metadata. FileShare.Read denies
writers/deletion while that handle is held. It never loads these files as
assemblies, gets a runtime Type, executes IL/methods/custom attributes/initializers,
constructs a requested target instance, reflects live private fields, reads field
values/offsets, scans the environment, or launches a process/thread/window/device.
Its own ordinary host/metadata/DTO constructors are distinct from target instances.
Only TypeDef names, declared field/base/signature/ClassLayout metadata and generic
parameter names are read. The existing bounded file extents bound the metadata
walk. Existing signature/nesting/TypeSpec cycle refusals remain unchanged.

AutoLayout/packing/declared size and generic signatures are metadata declarations,
not actual object size/offset/padding or allocation observations. The new fields
can select a compiled branch for a later separate source/ABI equation; they do
not certify that equation or GC reclamation. No R11 executable or protocol data
is inspected or modified. All original dependencies and exact file pins remain
in source-pins.json. Root owns any future compiler/reference/host/timeout/capture
runner after independent review. There is no new runner in this source packet.
The already frozen revision1 runner is neither edited nor blindly repointed.

Authoring used only Python source text/JSON/hash comparisons. No C# compiler,
Add-Type, target parser/AST/import/helper/program/protocol stage, metadata reader,
GUI/device, Git or CI was invoked. The entire new reader/README/pins must freeze
at its future first actual GREEN execution; no such execution has occurred here.
'''
(OUT/'README.md').write_text(readme,encoding='utf-8',newline='\n')
(OUT/'evidence/reader.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),source.splitlines(True),fromfile='frozen-r1/reader.cs',tofile='unrun-r2/reader.cs')),encoding='utf-8',newline='\n')
def pin(p):
    d=p.read_bytes()
    if b'\r' in d:raise ValueError('authored CR byte')
    return dict(path=p.relative_to(ROOT).as_posix(),bytes=len(d),sha256=hashlib.sha256(d).hexdigest().upper())
pins=json.loads((BASE/'source-pins.json').read_text())
pins['schema']='FIXED_PUBLIC_FILE_METADATA_INVENTORY_SOURCE_PINS_R2'
pins['status']='SOURCE_ONLY_UNCOMPILED_UNRUN_REQUIRES_INDEPENDENT_WHOLE_REVIEW'
pins['allowlist_original_required']=pins.pop('allowlist')
pins['optional_named_types']=['System.Collections.Generic.List`1+Enumerator','System.ArrayEnumerator','System.SZGenericArrayEnumerator`1']
pins['generated_selection']='Direct declared JObject child with exact parent+<GetEnumerator>d__ prefix and nonempty decimal suffix; exactly one or FAIL.'
pins['counts'].update(required_named_types=21,optional_named_types=3,required_generated_selections=1,reported_types_min=22,reported_types_max=25)
pins['authority_references'] += [pin(BASE/'reader.cs'),pin(BASE/'source-pins.json'),pin(BASE/'execution-r2/inventory.json')]
pins['authored_sources']=[pin(OUT/'reader.cs'),pin(OUT/'README.md'),pin(Path(__file__).resolve()),pin(OUT/'evidence/reader.diff')]
pins['source_checks']=dict(original21_required_type_literals_preserved=True,original4_input_length_hash_constants_preserved=True,only_requested_name_and_enum_metadata_added=True,all_authored_UTF8_LF=True,reader_not_compiled_or_run=True)
pins['execution']={k:False for k in pins['execution']}
(OUT/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(source=pin(OUT/'reader.cs'),pins=pin(OUT/'source-pins.json'),readme=pin(OUT/'README.md')),indent=2))
