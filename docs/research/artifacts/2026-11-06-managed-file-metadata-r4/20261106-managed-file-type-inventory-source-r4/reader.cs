#nullable enable
using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace YesDaw.FileMetadataInventory;

public static class Program
{
    private const string RuntimeRoot = "C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/";
    private sealed record Input(string File, long Length, string Sha256, string[] Types);
    private static readonly Input[] Inputs =
    {
        new("Newtonsoft.Json.dll", 1824600, "9AF3870D53662BA1C8E9B19EC8AD0020E815FFE9E916A1F3204AC6E206447F60", new[]
        {
            "Newtonsoft.Json.Linq.JToken",
            "Newtonsoft.Json.Linq.JContainer",
            "Newtonsoft.Json.Linq.JObject",
            "Newtonsoft.Json.Linq.JArray",
            "Newtonsoft.Json.Linq.JProperty",
            "Newtonsoft.Json.Linq.JProperty+JPropertyList",
            "Newtonsoft.Json.Linq.JValue",
            "Newtonsoft.Json.Linq.JPropertyKeyedCollection",
            "Newtonsoft.Json.Linq.JTokenWriter",
            "Newtonsoft.Json.JsonReader",
            "Newtonsoft.Json.JsonTextReader",
            "Newtonsoft.Json.JsonWriter",
            "Newtonsoft.Json.JsonTextWriter",
            "Newtonsoft.Json.JsonPosition",
            "Newtonsoft.Json.Utilities.StringBuffer",
            "Newtonsoft.Json.Utilities.StringReference"
        }),
        new("System.Management.Automation.dll", 19646264, "2D8AF026E589C5603629BFC3E7C57265B4167B12ECA3AE549C6A6C3CA7E4967B", new[]
        {
            "System.Management.Automation.OrderedHashtable"
        }),
        new("System.Collections.Specialized.dll", 104232, "61702E1173C49F32E7B1804667A249F8709680F01859D46CF30E74197FEDC1C5", new[]
        {
            "System.Collections.Specialized.OrderedDictionary"
        }),
        new("System.Private.CoreLib.dll", 16033616, "ED80A1A5DD066C0407AD59686F9AF90A4F3E55DBF3E4C973B53CC8C9D0706F82", new[]
        {
            "System.Tuple`5",
            "System.Collections.ArrayList",
            "System.Collections.DictionaryEntry",
            "System.Tuple`2",
            "System.IO.Stream",
            "System.IO.FileStream",
            "System.IO.StreamReader",
            "System.IO.Strategies.FileStreamStrategy",
            "System.IO.Strategies.OSFileStreamStrategy",
            "System.IO.Strategies.SyncWindowsFileStreamStrategy",
            "System.IO.Strategies.BufferedFileStreamStrategy",
            "System.IO.Stream+<ReadAtLeastAsyncCore>d__49",
            "System.IO.Strategies.BufferedFileStreamStrategy+<ReadFromNonSeekableAsync>d__36",
            "System.Threading.AsyncOverSyncWithIoCancellation",
            "System.Threading.AsyncOverSyncWithIoCancellation+SyncAsyncWorkItemRegistration",
            "System.Threading.AsyncOverSyncWithIoCancellation+<InvokeAsync>d__8`2",
            "System.Threading.CancellationTokenSource",
            "System.Threading.CancellationTokenSource+Registrations",
            "System.Threading.CancellationTokenSource+CallbackNode",
            "System.Threading.CancellationTokenRegistration",
            "System.Threading.CancellationToken",
            "System.Threading.SemaphoreSlim",
            "System.Threading.SemaphoreSlim+TaskNode",
            "System.Threading.Tasks.Task",
            "System.Threading.Tasks.Task`1",
            "System.Threading.Tasks.ValueTask`1",
            "System.Threading.Tasks.ValueTask`1+ValueTaskSourceAsTask",
            "System.Runtime.CompilerServices.PoolingAsyncValueTaskMethodBuilder`1",
            "System.Runtime.CompilerServices.PoolingAsyncValueTaskMethodBuilder`1+StateMachineBox",
            "System.Runtime.CompilerServices.PoolingAsyncValueTaskMethodBuilder`1+StateMachineBox`1",
            "System.Runtime.CompilerServices.AsyncTaskMethodBuilder`1",
            "System.Runtime.CompilerServices.AsyncTaskMethodBuilder`1+AsyncStateMachineBox`1",
            "System.Runtime.CompilerServices.AsyncValueTaskMethodBuilder`1",
            "System.Threading.Tasks.Sources.ManualResetValueTaskSourceCore`1",
            "System.Runtime.InteropServices.SafeHandle",
            "Microsoft.Win32.SafeHandles.SafeHandleZeroOrMinusOneIsInvalid",
            "Microsoft.Win32.SafeHandles.SafeFileHandle",
            "Microsoft.Win32.SafeHandles.SafeThreadHandle",
            "System.Memory`1",
            "System.ValueTuple`3",
            "System.Text.Encoding",
            "System.Text.Decoder",
            "System.Text.DecoderNLS",
            "System.Diagnostics.Stopwatch",
            "System.MarshalByRefObject"
        }),
        new("System.Diagnostics.Process.dll", 329512, "2B4871E02B366113C6CCF3B1436FADDE5E69FC0A3E3AC421E2FA0F7AF8C2BC3B", new[]
        {
            "System.Diagnostics.Process",
            "System.Diagnostics.ProcessStartInfo",
            "System.Collections.Specialized.DictionaryWrapper",
            "Microsoft.Win32.SafeHandles.SafeProcessHandle",
            "Microsoft.Win32.SafeHandles.SafeThreadHandle",
            "System.Text.ConsoleEncoding",
            "System.Text.OSEncoding",
            "System.Text.DecoderDBCS"
        }),
        new("System.Security.Cryptography.dll", 2549584, "BEE61E6BDEFF9FB0C4F3CBF9D48E9D1D97D12A8BB85D1EEAB9178E818B582EEE", new[]
        {
            "System.Security.Cryptography.IncrementalHash",
            "System.Security.Cryptography.HashProvider",
            "System.Security.Cryptography.HashProviderCng",
            "System.Security.Cryptography.ConcurrencyBlock",
            "Microsoft.Win32.SafeHandles.SafeBCryptHandle",
            "Microsoft.Win32.SafeHandles.SafeBCryptHashHandle",
            "Microsoft.Win32.SafeHandles.SafeBCryptAlgorithmHandle"
        }),
        new("System.Text.Encoding.CodePages.dll", 862032, "56582910B3C0D17F10DC2EAFC8E5746C6F23B57AF56D7F0D4CD208F06ECE1E28", new[]
        {
            "System.Text.EncodingNLS",
            "System.Text.BaseCodePageEncoding",
            "System.Text.SBCSCodePageEncoding",
            "System.Text.DBCSCodePageEncoding",
            "System.Text.DecoderNLS"
        }),
        new("System.ComponentModel.Primitives.dll", 79696, "B671AE0E9463F65D2AD08366C890043FD570F7D3149BB57BC16B8D427D9684F5", new[]
        {
            "System.ComponentModel.Component"
        })
    };

    private static readonly string[] OptionalCoreTypes =
    {
        "System.Collections.Generic.List`1+Enumerator",
        "System.ArrayEnumerator",
        "System.SZGenericArrayEnumerator`1"
    };
    private sealed record NestedDiscovery(string File, string Parent, string[] Names, string Selected);
    private sealed record OptionalPresence(string File, string Type, bool Present);

    private sealed class InventoryFailure : Exception
    {
        internal readonly string Reason;
        internal InventoryFailure(string reason) { Reason = reason; }
    }

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length != 0) throw new InventoryFailure("ARGUMENTS_NOT_ALLOWED");
            byte[] output = ReadAll();
            using Stream stdout = Console.OpenStandardOutput();
            stdout.Write(output, 0, output.Length);
            return 0;
        }
        catch (InventoryFailure e) { return Fail("InventoryFailure", e.Reason); }
        catch (BadImageFormatException) { return Fail("BadImageFormatException", "INVALID_METADATA_OR_SIGNATURE"); }
        catch (UnauthorizedAccessException) { return Fail("UnauthorizedAccessException", "READ_ACCESS"); }
        catch (IOException) { return Fail("IOException", "FILE_OR_OUTPUT_IO"); }
        catch (NotSupportedException) { return Fail("NotSupportedException", "UNSUPPORTED_METADATA"); }
        catch (ArgumentException) { return Fail("ArgumentException", "INVALID_METADATA_ARGUMENT"); }
        catch (Exception) { return Fail("Exception", "UNEXPECTED_HOST_FAILURE"); }
    }

    private static int Fail(string exceptionType, string reason)
    {
        // Both values come only from fixed source literals, never exception text.
        Console.Error.WriteLine("{\"status\":\"FAIL\",\"exception_type\":\"" + exceptionType + "\",\"reason\":\"" + reason + "\"}");
        return 1;
    }

    private static byte[] ReadAll()
    {
        using var buffer = new MemoryStream();
        using (var json = new Utf8JsonWriter(buffer, new JsonWriterOptions { Indented = false }))
        {
            json.WriteStartObject();
            json.WriteString("schema", "FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V4");
            json.WriteString("status", "PASS_FILE_METADATA_ONLY");
            json.WriteNumber("required_named_types", 84);
            json.WriteNumber("optional_named_types", 3);
            json.WriteNumber("required_generated_type_selections", 1);
            json.WriteBoolean("target_assemblies_explicitly_loaded_or_executed", false);
            json.WriteStartArray("types");
            int count = 0;
            int requiredCount = 0;
            int generatedCount = 0;
            var discoveries = new List<NestedDiscovery>();
            var optionalPresence = new List<OptionalPresence>();
            foreach (Input input in Inputs)
            {
                // Same read-only handle is pinned then parsed; FileShare.Read denies writers/deletion.
                using var file = new FileStream(RuntimeRoot + input.File, FileMode.Open, FileAccess.Read, FileShare.Read);
                if (file.Length != input.Length) throw new InventoryFailure("INPUT_LENGTH_MISMATCH");
                if (!String.Equals(Convert.ToHexString(SHA256.HashData(file)), input.Sha256, StringComparison.Ordinal))
                    throw new InventoryFailure("INPUT_HASH_MISMATCH");
                file.Position = 0;
                using var pe = new PEReader(file, PEStreamOptions.PrefetchMetadata | PEStreamOptions.LeaveOpen);
                if (!pe.HasMetadata) throw new InventoryFailure("NO_MANAGED_METADATA");
                MetadataReader reader = pe.GetMetadataReader();
                if (!reader.IsAssembly) throw new InventoryFailure("NOT_ASSEMBLY_METADATA");
                var provider = new SignatureNames(reader);
                var wanted = new Dictionary<string, TypeDefinitionHandle>(StringComparer.Ordinal);
                foreach (string type in input.Types) wanted.Add(type, default);
                bool core = String.Equals(input.File, "System.Private.CoreLib.dll", StringComparison.Ordinal);
                if (core) foreach (string type in OptionalCoreTypes) wanted.Add(type, default);
                // This walks only TypeDef name metadata, not methods, IL, attributes or executable code.
                foreach (TypeDefinitionHandle handle in reader.TypeDefinitions)
                {
                    string name = provider.DefinitionName(handle);
                    if (!wanted.TryGetValue(name, out TypeDefinitionHandle prior)) continue;
                    if (!prior.IsNil) throw new InventoryFailure("DUPLICATE_REQUESTED_TYPE");
                    wanted[name] = handle;
                }
                foreach (string type in input.Types)
                {
                    TypeDefinitionHandle handle = wanted[type];
                    if (handle.IsNil) throw new InventoryFailure("REQUESTED_TYPE_MISSING");
                    WriteType(json, input, reader, provider, handle, type);
                    count++;
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
            if (requiredCount != 84 || generatedCount != 1 || discoveries.Count != 1 || optionalPresence.Count != 3
                || count < 85 || count > 88) throw new InventoryFailure("ALLOWLIST_COUNT");
            json.WriteEndArray();
            json.WriteNumber("reported_types", count);
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
            json.WriteEndObject();
        }
        // Partial metadata output is never published on failure.
        buffer.WriteByte(10);
        return buffer.ToArray();
    }

    private static void WriteType(Utf8JsonWriter json, Input input, MetadataReader reader,
        SignatureNames provider, TypeDefinitionHandle handle, string qualifiedName)
    {
        TypeDefinition type = reader.GetTypeDefinition(handle);
        var context = new GenericContext(reader, type);
        TypeLayout layout = type.GetLayout();
        TypeAttributes layoutKind = type.Attributes & TypeAttributes.LayoutMask;
        string layoutName = layoutKind == TypeAttributes.AutoLayout ? "AutoLayout"
            : layoutKind == TypeAttributes.SequentialLayout ? "SequentialLayout"
            : layoutKind == TypeAttributes.ExplicitLayout ? "ExplicitLayout"
            : throw new InventoryFailure("UNSUPPORTED_LAYOUT_FLAGS");
        json.WriteStartObject();
        json.WriteString("file", input.File);
        json.WriteNumber("file_bytes", input.Length);
        json.WriteString("file_sha256", input.Sha256);
        json.WriteString("assembly_name", provider.AssemblyName);
        json.WriteString("qualified_name", qualifiedName);
        json.WriteString("metadata_token", Token(handle));
        json.WriteString("type_attributes", type.Attributes.ToString());
        json.WriteString("type_attributes_hex", "0x" + ((uint)type.Attributes).ToString("X8", CultureInfo.InvariantCulture));
        json.WriteString("layout_kind", layoutName);
        json.WriteBoolean("class_layout_metadata_is_default", layout.IsDefault);
        json.WriteNumber("metadata_packing_size", layout.PackingSize);
        json.WriteNumber("metadata_declared_size", layout.Size);
        EntityHandle baseType = type.BaseType;
        json.WriteString("base_handle_kind", baseType.IsNil ? "Nil" : baseType.Kind.ToString());
        json.WriteString("base_metadata_token", baseType.IsNil ? "" : Token(baseType));
        json.WriteString("base_decoded", baseType.IsNil ? "" : provider.EntityName(baseType, context));
        json.WriteString("base_signature_blob_hex", baseType.Kind == HandleKind.TypeSpecification
            ? Convert.ToHexString(reader.GetBlobBytes(reader.GetTypeSpecification((TypeSpecificationHandle)baseType).Signature)) : "");
        json.WriteStartArray("generic_parameters");
        foreach (string name in context.Names) json.WriteStringValue(name);
        json.WriteEndArray();
        json.WriteStartArray("declared_nonstatic_instance_fields");
        int fields = 0;
        foreach (FieldDefinitionHandle fieldHandle in type.GetFields())
        {
            FieldDefinition field = reader.GetFieldDefinition(fieldHandle);
            if ((field.Attributes & FieldAttributes.Static) != 0) continue;
            string signature = field.DecodeSignature(provider, context);
            json.WriteStartObject();
            json.WriteString("name", reader.GetString(field.Name));
            json.WriteString("metadata_token", Token(fieldHandle));
            json.WriteString("attributes", field.Attributes.ToString());
            json.WriteString("attributes_hex", "0x" + ((uint)field.Attributes).ToString("X8", CultureInfo.InvariantCulture));
            json.WriteString("decoded_signature", signature);
            json.WriteString("signature_blob_hex", Convert.ToHexString(reader.GetBlobBytes(field.Signature)));
            json.WriteEndObject();
            fields++;
        }
        json.WriteEndArray();
        json.WriteNumber("declared_nonstatic_instance_field_count", fields);
        json.WriteEndObject();
    }

    private static string Token(EntityHandle handle) => "0x" + MetadataTokens.GetToken(handle).ToString("X8", CultureInfo.InvariantCulture);

    private sealed class GenericContext
    {
        internal readonly string[] Names;
        internal GenericContext(MetadataReader reader, TypeDefinition type)
        {
            GenericParameterHandleCollection parameters = type.GetGenericParameters();
            Names = new string[parameters.Count];
            foreach (GenericParameterHandle handle in parameters)
            {
                GenericParameter parameter = reader.GetGenericParameter(handle);
                int index = parameter.Index;
                if ((uint)index >= (uint)Names.Length || Names[index] != null) throw new InventoryFailure("GENERIC_PARAMETER_INDEX");
                Names[index] = reader.GetString(parameter.Name);
            }
            foreach (string name in Names) if (name == null) throw new InventoryFailure("GENERIC_PARAMETER_GAP");
        }
    }

    private sealed class SignatureNames : ISignatureTypeProvider<string, GenericContext>
    {
        private readonly MetadataReader reader;
        private readonly HashSet<TypeSpecificationHandle> activeSpecifications = new();
        internal readonly string AssemblyName;
        internal SignatureNames(MetadataReader metadata)
        {
            reader = metadata;
            AssemblyName = reader.GetString(reader.GetAssemblyDefinition().Name);
        }

        internal string DefinitionName(TypeDefinitionHandle handle)
        {
            var seen = new HashSet<TypeDefinitionHandle>();
            var parts = new List<string>();
            string ns = "";
            while (!handle.IsNil)
            {
                if (!seen.Add(handle)) throw new InventoryFailure("TYPE_NESTING_CYCLE");
                TypeDefinition type = reader.GetTypeDefinition(handle);
                parts.Add(reader.GetString(type.Name));
                ns = reader.GetString(type.Namespace);
                handle = type.GetDeclaringType();
            }
            parts.Reverse();
            return (ns.Length == 0 ? "" : ns + ".") + String.Join("+", parts);
        }

        private string ReferenceName(TypeReferenceHandle handle)
        {
            var seen = new HashSet<TypeReferenceHandle>();
            var parts = new List<string>();
            string ns = "";
            EntityHandle scope;
            for (;;)
            {
                if (!seen.Add(handle)) throw new InventoryFailure("TYPE_REFERENCE_CYCLE");
                TypeReference type = reader.GetTypeReference(handle);
                parts.Add(reader.GetString(type.Name));
                ns = reader.GetString(type.Namespace);
                scope = type.ResolutionScope;
                if (scope.Kind != HandleKind.TypeReference) break;
                handle = (TypeReferenceHandle)scope;
            }
            string assembly = scope.Kind == HandleKind.AssemblyReference
                ? reader.GetString(reader.GetAssemblyReference((AssemblyReferenceHandle)scope).Name)
                : scope.Kind == HandleKind.ModuleDefinition ? AssemblyName
                : scope.Kind == HandleKind.ModuleReference ? "module:" + reader.GetString(reader.GetModuleReference((ModuleReferenceHandle)scope).Name)
                : throw new InventoryFailure("UNSUPPORTED_TYPE_REFERENCE_SCOPE");
            parts.Reverse();
            return "[" + assembly + "]" + (ns.Length == 0 ? "" : ns + ".") + String.Join("+", parts);
        }

        internal string EntityName(EntityHandle handle, GenericContext context) => handle.Kind switch
        {
            HandleKind.TypeDefinition => "[" + AssemblyName + "]" + DefinitionName((TypeDefinitionHandle)handle),
            HandleKind.TypeReference => ReferenceName((TypeReferenceHandle)handle),
            HandleKind.TypeSpecification => GetTypeFromSpecification(reader, context, (TypeSpecificationHandle)handle, 0),
            _ => throw new InventoryFailure("UNSUPPORTED_TYPE_HANDLE")
        };
        private static string Kind(byte raw) => raw == 0 ? "type " : raw == 0x11 ? "valuetype " : raw == 0x12 ? "class " : throw new InventoryFailure("UNSUPPORTED_SIGNATURE_TYPE_KIND");
        public string GetTypeFromDefinition(MetadataReader _, TypeDefinitionHandle handle, byte rawTypeKind) => Kind(rawTypeKind) + "[" + AssemblyName + "]" + DefinitionName(handle);
        public string GetTypeFromReference(MetadataReader _, TypeReferenceHandle handle, byte rawTypeKind) => Kind(rawTypeKind) + ReferenceName(handle);
        public string GetTypeFromSpecification(MetadataReader _, GenericContext context, TypeSpecificationHandle handle, byte rawTypeKind)
        {
            Kind(rawTypeKind);
            if (!activeSpecifications.Add(handle)) throw new InventoryFailure("TYPE_SPECIFICATION_CYCLE");
            try { return reader.GetTypeSpecification(handle).DecodeSignature(this, context); }
            finally { activeSpecifications.Remove(handle); }
        }
        public string GetGenericTypeParameter(GenericContext context, int index) => (uint)index < (uint)context.Names.Length
            ? "!" + index.ToString(CultureInfo.InvariantCulture) + "(" + context.Names[index] + ")"
            : throw new InventoryFailure("GENERIC_TYPE_PARAMETER_INDEX");
        public string GetGenericMethodParameter(GenericContext _, int index) => index >= 0 ? "!!" + index.ToString(CultureInfo.InvariantCulture) : throw new InventoryFailure("GENERIC_METHOD_PARAMETER_INDEX");
        public string GetGenericInstantiation(string genericType, ImmutableArray<string> typeArguments) => genericType + "<" + String.Join(",", typeArguments) + ">";
        public string GetArrayType(string elementType, ArrayShape shape) => elementType + "[rank=" + shape.Rank.ToString(CultureInfo.InvariantCulture) + ";sizes=" + String.Join(",", shape.Sizes) + ";lower=" + String.Join(",", shape.LowerBounds) + "]";
        public string GetSZArrayType(string elementType) => elementType + "[]";
        public string GetByReferenceType(string elementType) => elementType + "&";
        public string GetPointerType(string elementType) => elementType + "*";
        public string GetPinnedType(string elementType) => "pinned " + elementType;
        public string GetModifiedType(string modifier, string unmodifiedType, bool isRequired) => (isRequired ? "modreq(" : "modopt(") + modifier + ") " + unmodifiedType;
        public string GetFunctionPointerType(MethodSignature<string> signature) => "fnptr[header=0x" + signature.Header.RawValue.ToString("X2", CultureInfo.InvariantCulture)
            + ";generic=" + signature.GenericParameterCount.ToString(CultureInfo.InvariantCulture) + ";required=" + signature.RequiredParameterCount.ToString(CultureInfo.InvariantCulture)
            + "](" + String.Join(",", signature.ParameterTypes) + ")->" + signature.ReturnType;
        public string GetPrimitiveType(PrimitiveTypeCode code) => code switch
        {
            PrimitiveTypeCode.Void => "void", PrimitiveTypeCode.Boolean => "bool", PrimitiveTypeCode.Char => "char",
            PrimitiveTypeCode.SByte => "int8", PrimitiveTypeCode.Byte => "uint8", PrimitiveTypeCode.Int16 => "int16",
            PrimitiveTypeCode.UInt16 => "uint16", PrimitiveTypeCode.Int32 => "int32", PrimitiveTypeCode.UInt32 => "uint32",
            PrimitiveTypeCode.Int64 => "int64", PrimitiveTypeCode.UInt64 => "uint64", PrimitiveTypeCode.Single => "float32",
            PrimitiveTypeCode.Double => "float64", PrimitiveTypeCode.String => "string", PrimitiveTypeCode.Object => "object",
            PrimitiveTypeCode.IntPtr => "native int", PrimitiveTypeCode.UIntPtr => "native uint", PrimitiveTypeCode.TypedReference => "typedref",
            _ => throw new InventoryFailure("UNSUPPORTED_PRIMITIVE_SIGNATURE")
        };
    }
}
