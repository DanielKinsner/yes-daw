// Source-first, separate-purpose ABI probe. Do not run before whole-source review.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Runtime;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

public static class ManagedPayloadAbiProbeR1
{
    private const long AllocationBudget = 8L * 1024 * 1024;
    private const int Repeats = 3;
    private const int CasesPerPass = 52;
    // No initialized object graph or dynamic static constructor. Escape is after
    // the second counter; this is not a claim that earlier results stay live.
    private static object s_sink;

    private enum Family { Byte, Char, Int, Reference, String, BucketInt, DictObject, DictString, Hashtable }

    private struct Row
    {
        public Family Family;
        public int Requested;
        public int Repeat;
        public int PublicCapacity;
        public long Before;
        public long After;
        public long Delta { get { return After - Before; } }
    }

    private static long Align8(long value) { return (value + 7) & ~7L; }

    // Only the four fixed positive Hashtable constructor arguments use this.
    // The division is float, then widened to double, as in the pinned source.
    private static int HashtableCapacity(int requested)
    {
        double raw = requested / (0.72f * 1.0f);
        int minimum = (int)raw;
        if (raw <= 3 || minimum <= 3) return 3;
        if (minimum <= 7) return 7;
        if (minimum <= 11) return 11;
        if (minimum <= 17) return 17;
        throw new InvalidOperationException("FIXED_HASHTABLE_DOMAIN");
    }

    private static long Expected(Family family, int requested)
    {
        switch (family)
        {
            case Family.Byte: return Align8(24L + requested);
            case Family.Char: return Align8(24L + 2L * requested);
            case Family.Int:
            case Family.BucketInt: return Align8(24L + 4L * requested);
            case Family.Reference: return Align8(24L + 8L * requested);
            case Family.String: return Align8(22L + 2L * requested);
            case Family.DictObject:
            case Family.DictString:
                return 80L + Align8(24L + 4L * requested) + Align8(24L + 24L * requested);
            case Family.Hashtable: return 72L + Align8(24L + 24L * HashtableCapacity(requested));
            default: throw new InvalidOperationException("FIXED_FAMILY_DOMAIN");
        }
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static Row Measure(Family family, int requested, int repeat, IEqualityComparer ordinalIgnoreCase)
    {
        object value;
        int publicCapacity = -1;
        // Nothing in this interval formats output, creates a delegate, boxes a
        // value, or touches a result container. Dictionary capacity queries are
        // deliberately INCLUDED, consistently for warmup and recorded repeats.
        long before = GC.GetAllocatedBytesForCurrentThread();
        switch (family)
        {
            case Family.Byte: value = new byte[requested]; break;
            case Family.Char: value = new char[requested]; break;
            case Family.Int:
            case Family.BucketInt: value = new int[requested]; break;
            case Family.Reference: value = new object[requested]; break;
            case Family.String: value = new string('x', requested); break;
            case Family.DictObject:
                var objectDictionary = new Dictionary<string, object>(requested);
                publicCapacity = objectDictionary.EnsureCapacity(0);
                value = objectDictionary;
                break;
            case Family.DictString:
                var stringDictionary = new Dictionary<string, string>(requested);
                publicCapacity = stringDictionary.EnsureCapacity(0);
                value = stringDictionary;
                break;
            case Family.Hashtable: value = new Hashtable(requested, ordinalIgnoreCase); break;
            default: throw new InvalidOperationException("FIXED_FAMILY_DOMAIN");
        }
        long after = GC.GetAllocatedBytesForCurrentThread();
        s_sink = value;
        GC.KeepAlive(value);
        return new Row { Family = family, Requested = requested, Repeat = repeat,
            PublicCapacity = publicCapacity, Before = before, After = after };
    }

    private static long Pass(int[] sizes, int[] primes, int[] hashSizes,
        IEqualityComparer comparer, Row[] rows, int repeat, ref int count)
    {
        long predicted = 0;
        for (int family = (int)Family.Byte; family <= (int)Family.String; ++family)
        {
            foreach (int size in sizes)
            {
                Row row = Measure((Family)family, size, repeat, comparer);
                predicted += Expected((Family)family, size);
                if (rows != null) rows[count++] = row;
            }
        }
        foreach (int prime in primes)
        {
            Row row = Measure(Family.BucketInt, prime, repeat, comparer);
            predicted += Expected(Family.BucketInt, prime);
            if (rows != null) rows[count++] = row;
        }
        for (int family = (int)Family.DictObject; family <= (int)Family.DictString; ++family)
        {
            foreach (int prime in primes)
            {
                Row row = Measure((Family)family, prime, repeat, comparer);
                predicted += Expected((Family)family, prime);
                if (rows != null) rows[count++] = row;
            }
        }
        foreach (int size in hashSizes)
        {
            Row row = Measure(Family.Hashtable, size, repeat, comparer);
            predicted += Expected(Family.Hashtable, size);
            if (rows != null) rows[count++] = row;
        }
        return predicted;
    }

    private static Row Find(Row[] rows, Family family, int requested, int repeat)
    {
        foreach (Row row in rows)
            if (row.Family == family && row.Requested == requested && row.Repeat == repeat) return row;
        throw new InvalidOperationException("FIXED_ROW_MISSING");
    }

    private static void Check(bool condition, ref int checks, ref int failures)
    {
        ++checks;
        if (!condition) ++failures;
    }

    private static void EmitRow(Row row)
    {
        int predictedCapacity = row.Family == Family.Hashtable ? HashtableCapacity(row.Requested) :
            (row.Family == Family.DictObject || row.Family == Family.DictString ? row.Requested : -1);
        Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
            "{{\"kind\":\"measurement\",\"family\":\"{0}\",\"requested\":{1},\"repeat\":{2},\"public_capacity\":{3},\"source_predicted_capacity\":{4},\"before\":{5},\"after\":{6},\"delta\":{7},\"source_predicted_delta\":{8}}}",
            row.Family, row.Requested, row.Repeat, row.PublicCapacity, predictedCapacity,
            row.Before, row.After, row.Delta, Expected(row.Family, row.Requested)));
    }

    public static int Run()
    {
        try
        {
            long runStart = GC.GetAllocatedBytesForCurrentThread();
            const string informational = "10.0.11-servicing.26373.116+e2f47b0110ed922f21a1522da67279133ce28f32";
            var coreInfo = typeof(object).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>();
            if (!OperatingSystem.IsWindows() || IntPtr.Size != 8 ||
                RuntimeInformation.ProcessArchitecture != Architecture.X64 ||
                RuntimeInformation.RuntimeIdentifier != "win-x64" ||
                RuntimeInformation.FrameworkDescription != ".NET 10.0.11" ||
                Environment.Version.ToString() != "10.0.11" || GCSettings.IsServerGC ||
                coreInfo == null || coreInfo.InformationalVersion != informational)
            {
                Console.WriteLine("{\"kind\":\"summary\",\"exit\":2,\"reason\":\"UNSUPPORTED_RUNTIME\"}");
                return 2;
            }

            int[] sizes = { 1, 7, 17, 64, 435, 512 };
            int[] primes = { 3, 7, 17, 37, 197, 1931 };
            int[] hashSizes = { 2, 3, 5, 10 };
            IEqualityComparer comparer = StringComparer.OrdinalIgnoreCase;
            var rows = new Row[CasesPerPass * Repeats];
            int count = 0;
            long predictedFixtures = 0;
            // Exercise the exact method/constructor/generic/comparer/capacity
            // paths twice before the three credited repeats. Warmup deltas are
            // not assertions or evidence rows. Their allocations remain inside
            // the whole-Run allocation-budget observation.
            for (int warm = 0; warm < 2; ++warm)
                predictedFixtures += Pass(sizes, primes, hashSizes, comparer, null, -1, ref count);
            long warmupEnd = GC.GetAllocatedBytesForCurrentThread();
            for (int repeat = 0; repeat < Repeats; ++repeat)
                predictedFixtures += Pass(sizes, primes, hashSizes, comparer, rows, repeat, ref count);
            long measurementsEnd = GC.GetAllocatedBytesForCurrentThread();

            int checks = 0;
            int failures = 0;
            Check(count == rows.Length, ref checks, ref failures);
            foreach (Row row in rows)
            {
                Check(row.After >= row.Before, ref checks, ref failures);
                Check(row.Delta == Expected(row.Family, row.Requested), ref checks, ref failures);
                bool dictionary = row.Family == Family.DictObject || row.Family == Family.DictString;
                Check(row.PublicCapacity == (dictionary ? row.Requested : -1), ref checks, ref failures);
                EmitRow(row);
            }

            for (int repeat = 0; repeat < Repeats; ++repeat)
            {
                for (int family = (int)Family.Byte; family <= (int)Family.Reference; ++family)
                {
                    long difference = Find(rows, (Family)family, 512, repeat).Delta - Find(rows, (Family)family, 64, repeat).Delta;
                    long stride = difference / (512 - 64);
                    long header = Find(rows, (Family)family, 64, repeat).Delta - 64 * stride;
                    long expectedStride = 1L << family;
                    Check(difference % (512 - 64) == 0, ref checks, ref failures);
                    Check(stride == expectedStride, ref checks, ref failures);
                    Check(header == 24, ref checks, ref failures);
                    Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
                        "{{\"kind\":\"derived_array\",\"family\":\"{0}\",\"repeat\":{1},\"stride\":{2},\"header\":{3},\"tested_alignment\":8}}", (Family)family, repeat, stride, header));
                }
                long measuredHeader = Find(rows, Family.Int, 64, repeat).Delta - 4L * 64;
                for (int family = (int)Family.DictObject; family <= (int)Family.DictString; ++family)
                {
                    long baseResidual = Find(rows, (Family)family, 3, repeat).Delta - Find(rows, Family.BucketInt, 3, repeat).Delta;
                    long nextResidual = Find(rows, (Family)family, 7, repeat).Delta - Find(rows, Family.BucketInt, 7, repeat).Delta;
                    long difference = nextResidual - baseResidual;
                    long stride = difference / (7 - 3);
                    Check(difference % (7 - 3) == 0, ref checks, ref failures);
                    Check(stride == 24, ref checks, ref failures);
                    foreach (int prime in primes)
                    {
                        long residual = Find(rows, (Family)family, prime, repeat).Delta - Find(rows, Family.BucketInt, prime, repeat).Delta;
                        long fixedBytes = residual - measuredHeader - stride * prime;
                        Check(fixedBytes == 80, ref checks, ref failures);
                    }
                    long fixedObject = baseResidual - measuredHeader - stride * 3;
                    Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
                        "{{\"kind\":\"derived_dictionary\",\"family\":\"{0}\",\"repeat\":{1},\"entry_stride\":{2},\"fixed_object_bytes\":{3},\"measured_array_header\":{4}}}", (Family)family, repeat, stride, fixedObject, measuredHeader));
                }
                int h2 = HashtableCapacity(2);
                int h3 = HashtableCapacity(3);
                long hashDifference = Find(rows, Family.Hashtable, 3, repeat).Delta - Find(rows, Family.Hashtable, 2, repeat).Delta;
                long hashStride = hashDifference / (h3 - h2);
                Check(hashDifference % (h3 - h2) == 0, ref checks, ref failures);
                Check(hashStride == 24, ref checks, ref failures);
                foreach (int size in hashSizes)
                    Check(Find(rows, Family.Hashtable, size, repeat).Delta - measuredHeader - hashStride * HashtableCapacity(size) == 72, ref checks, ref failures);
                long hashFixed = Find(rows, Family.Hashtable, 2, repeat).Delta - measuredHeader - hashStride * h2;
                Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
                    "{{\"kind\":\"derived_hashtable\",\"repeat\":{0},\"bucket_stride\":{1},\"fixed_object_bytes\":{2},\"capacity_provenance\":\"SOURCE_PREDICTION_NOT_PUBLIC_OBSERVATION\"}}", repeat, hashStride, hashFixed));
            }

            GC.KeepAlive(s_sink);
            long outputEnd = GC.GetAllocatedBytesForCurrentThread();
            Check(runStart <= warmupEnd && warmupEnd <= measurementsEnd && measurementsEnd <= outputEnd, ref checks, ref failures);
            Check(predictedFixtures > 0 && predictedFixtures <= AllocationBudget, ref checks, ref failures);
            Check(outputEnd - runStart <= AllocationBudget, ref checks, ref failures);
            int exit = failures == 0 ? 0 : 1;
            Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
                "{{\"kind\":\"summary\",\"exit\":{0},\"checks\":{1},\"failures\":{2},\"measurement_rows\":{3},\"repeats\":{4},\"pointer_bytes\":{5},\"source_predicted_fixture_requests_all_five_passes\":{6},\"run_start\":{7},\"warmup_end\":{8},\"measurements_end\":{9},\"output_end_before_summary\":{10},\"observed_run_bytes_before_summary\":{11},\"allocation_budget\":{12},\"summary_precedes_final_budget_gate\":true,\"final_gate_requires_raw_exit\":true,\"runtime\":\"10.0.11\",\"claim\":\"FINITE_MANAGED_ALLOCATION_REQUEST_ABI_ONLY_NO_R10_FIT_OR_GO\"}}",
                exit, checks, failures, count, Repeats, IntPtr.Size, predictedFixtures, runStart,
                warmupEnd, measurementsEnd, outputEnd, outputEnd - runStart, AllocationBudget));
            // This actual final sample includes summary formatting and writing.
            // No source allocation occurs between this gate and normal return.
            // A summary alone never certifies success: root requires raw exit 0.
            long finalEnd = GC.GetAllocatedBytesForCurrentThread();
            if (finalEnd - runStart > AllocationBudget)
            {
                Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
                    "{{\"kind\":\"final_budget_failure\",\"exit\":1,\"run_start\":{0},\"after_summary\":{1},\"observed_run_bytes_through_summary\":{2},\"allocation_budget\":{3}}}",
                    runStart, finalEnd, finalEnd - runStart, AllocationBudget));
                return 1;
            }
            return exit;
        }
        catch (Exception exception)
        {
            // Bounded type name, no exception message, stack, environment or path.
            string name = exception.GetType().Name;
            if (name.Length > 96) name = "Exception";
            Console.WriteLine("{\"kind\":\"summary\",\"exit\":2,\"reason\":\"PROBE_EXCEPTION\",\"exception_type\":\"" + name + "\"}");
            return 2;
        }
    }
}
