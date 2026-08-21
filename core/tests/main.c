#include <stdio.h>

#include "test.h"

int manet_test_failures = 0;
int manet_test_checks = 0;

int main(void)
{
    printf("config\n");
    test_config_all();
    printf("addr\n");
    test_addr_all();
    printf("frame\n");
    test_frame_all();
    printf("slot\n");
    test_slot_all();
    printf("neighbour + mpr\n");
    test_mesh_all();
    test_suppression_is_coverage_based();
    test_sched_remembers_relayers();
    printf("dedup\n");
    test_dedup_all();
    printf("nama\n");
    test_nama_all();

    printf("\n%d checks, %d failures\n", manet_test_checks, manet_test_failures);
    return manet_test_failures == 0 ? 0 : 1;
}
