// Task shader entry point. Ensure that EmitMeshTasks
// is only statically called once, doing it any other
// way can trip up some compilers.
void main() [[subgroup_uniform_control_flow]] {
  uint count = TS_MAIN();
  EmitMeshTasksEXT(count, 1, 1);
}
