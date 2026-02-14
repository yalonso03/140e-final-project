# 140e-final-project


Labs that use checksums
- 3-cross-checking
- 5-threads
- 6-bootloader
- 7-uart

Proposal:
For our final project, we’d like to redesign the checksum system that we use in our labs. As we’ve discussed in lab, we currently validate our solutions by intercepting all calls to GET32 and PUT32 (implemented in lab 3) to produce a trace, and checking that the checksum of our trace matches that of a staff solution. Notably, this approach requires that all reads and writes appear in exactly the same order as the staff solution. This implementation is unnecessarily strict, as two implementations can both be fully correct, but differ in the order of some reads/writes. The current solution of requesting that students write all accesses in increasing memory address order is not ideal. So, we’d like to design a different approach that caters specifically to CS140e labs and devices used that would allow for the reordering of read/write operations (when doing so would be fully correct still). With this new approach, we will have an updated checksum tool that produces identical checksums for two functionally correct solutions, even if the order in which they access memory addresses differs from the staff solution (so long as important dependencies are respected). To implement this, we’ll model the execution as a memory dependency graph. We will start by assuming that all memory addresses are independent. So, we’ll only introduce dependencies when a read occurs after a write to the same memory location. However, this doesn’t model the real world; as in the ARM MMIO world, not all memory locations can be treated as independent (e.g. writing to one interrupt register might affect reads from some completely separate register). To deal with this, we’ll make it easy to add in custom rules based on specific constraints we’ve observed from doing the labs and reading the datasheets. Our checker will enforce these extra dependencies. It will also be easy to add in new rules so that this tool could be easily used in the future, even if labs/devices change. And finally, after completing the new checker, we’ll design a lab handout + lab starter code for the project so that students could implement this themselves in future quarters if they’re interested. 

