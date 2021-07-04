DefinitionBlock ("", "DSDT", 2, "DELL  ", "CBX3   ", 0x01072009)
{
  OperationRegion (GNVS, SystemMemory, 0xDB80E000, 0x072D)
  Field (GNVS, AnyAcc, Lock, Preserve)
  {
      GIRQ, 32,
      TMPL, 32,
  }

  Device (UAR0) {
      Method (DCRS, 1, NotSerialized) {
          TMPL = GIRQ (Arg0)
          Return (TMPL)
      }

      Method (GIRQ, 1, NotSerialized) {
          Return (Arg0)
      }
  }
}