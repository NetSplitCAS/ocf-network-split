#ifndef PMEM_NVME_TABLE_H
#define PMEM_NVME_TABLE_H

/* Use kernel types when building for kernel, standard types for userspace */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h> /* Add include for uint64_t type */
#endif

/* Bandwidth table structure:
 * First dimension: IO_Depth (1,2,4,8,16,32)
 * Second dimension: NumJob (1,2,4,8,16,32)
 * Third dimension: Split_Ratio (100,95,90,85,...,5,0)
 * Values represent Actual_BW
 */
static const int pmem_nvme_bw_table[6][6][21] = {
    /* IO_Depth 1 */
    {
        /* NumJob 1 */
        {53984, 23694, 13102, 10501, 8730, 7456, 6575, 5808, 5195, 4718, 4335, 4082, 3766, 3559, 3317, 3127, 2973, 2809, 2667, 2582, 2472},
        /* NumJob 2 */
        {88371, 47122, 26351, 21257, 17027, 14571, 12558, 11289, 9942, 8978, 8246, 7619, 7100, 6640, 6234, 5867, 5538, 5479, 5028, 4770, 4583},
        /* NumJob 4 */
        {112400, 94219, 51606, 39343, 32796, 27594, 23894, 21795, 19995, 17452, 16121, 14892, 14088, 12587, 11799, 11057, 10431, 9934, 9322, 9052, 8586},
        /* NumJob 8 */
        {108374, 116800, 95215, 75667, 63866, 55186, 49304, 41440, 38430, 34763, 31882, 29075, 26876, 24530, 23504, 22013, 20782, 19962, 18417, 18111, 16671},
        /* NumJob 16 */
        {77575, 85102, 94644, 103250, 131529, 125649, 109724, 94847, 81533, 74281, 66578, 59766, 55794, 51770, 48114, 46456, 42212, 40022, 37669, 38458, 34698},
        /* NumJob 32 */
        {62862, 71128, 73569, 79635, 88751, 96086, 104900, 151776, 133830, 118662, 107044, 96671, 88633, 81810, 76038, 71056, 66697, 62756, 59478, 56430, 53738}
    },
    /* IO_Depth 2 */
    {
        /* NumJob 1 */
        {53423, 54871, 56279, 47030, 25795, 17426, 12221, 11738, 9579, 9422, 8340, 7644, 7336, 6893, 6467, 6098, 5769, 5628, 5350, 5126, 4855},
        /* NumJob 2 */
        {87272, 92184, 90051, 73978, 37281, 30987, 25618, 22619, 18954, 18124, 16082, 15820, 13946, 13078, 12544, 11532, 10474, 10370, 10054, 9467, 9060},
        /* NumJob 4 */
        {112389, 118313, 119883, 120569, 68799, 54914, 47821, 45384, 39704, 33956, 32389, 29626, 27080, 25466, 24034, 22479, 20586, 19534, 18861, 18097, 16882},
        /* NumJob 8 */
        {108410, 114078, 121629, 128541, 137813, 110533, 94970, 88602, 75134, 66690, 61205, 57879, 53432, 49480, 46189, 52298, 40450, 38858, 35880, 34897, 33172},
        /* NumJob 16 */
        {77449, 82230, 87038, 92609, 100473, 111945, 118567, 153834, 135331, 119976, 107762, 97751, 89652, 82566, 76744, 71544, 67094, 63169, 59673, 56527, 53737},
        /* NumJob 32 */
        {62554, 66779, 69527, 77558, 85423, 92194, 99073, 111647, 132742, 119129, 107209, 97109, 88966, 82145, 76310, 71225, 66876, 62923, 59573, 56480, 53737}
    },
    /* IO_Depth 4 */
    {
        /* NumJob 1 */
        {53449, 55016, 56243, 58085, 59396, 61347, 60725, 54095, 46923, 43249, 40672, 37763, 31383, 24547, 24675, 20702, 10658, 21788, 21236, 15880, 14847},
        /* NumJob 2 */
        {89012, 93362, 95757, 98993, 101013, 107315, 93037, 86893, 49706, 50685, 47311, 46628, 39212, 35483, 34829, 32003, 24668, 28844, 26514, 22323, 18430},
        /* NumJob 4 */
        {112349, 118311, 124345, 131172, 127242, 145959, 147358, 134855, 85062, 73766, 66375, 67087, 55627, 52368, 48767, 50637, 45008, 43747, 38079, 36450, 39929},
        /* NumJob 8 */
        {108311, 113976, 120481, 127549, 136520, 145987, 156207, 154125, 135000, 119906, 107820, 97964, 89770, 82739, 76869, 71725, 67215, 63235, 59707, 56558, 53731},
        /* NumJob 16 */
        {77538, 82353, 87194, 92591, 99534, 106106, 114904, 124575, 135474, 119943, 107752, 97840, 89654, 82592, 76759, 71594, 67116, 63152, 59674, 56529, 53730},
        /* NumJob 32 */
        {62625, 66797, 68983, 75304, 81007, 87500, 95204, 108508, 113538, 119448, 107349, 97371, 89161, 82262, 76460, 71343, 66964, 62994, 59602, 56496, 53737}
    },
    /* IO_Depth 8 */
    {
        /* NumJob 1 */
        {52615, 53946, 55686, 57359, 60110, 60482, 63185, 64942, 65804, 69258, 70785, 64079, 60536, 57233, 55720, 54546, 50327, 49803, 45896, 45310, 44210},
        /* NumJob 2 */
        {88537, 91584, 96604, 99889, 100079, 106372, 111107, 113599, 122110, 116002, 106066, 96970, 89411, 82586, 76698, 71596, 67064, 63048, 59275, 55926, 52635},
        /* NumJob 4 */
        {111967, 118028, 124208, 131680, 127402, 149079, 159192, 153740, 134458, 119552, 107572, 97767, 89626, 82692, 76808, 71686, 67176, 63208, 59684, 56540, 53703},
        /* NumJob 8 */
        {108082, 113786, 120385, 127530, 136429, 145337, 155999, 154171, 135037, 119942, 107770, 97989, 89780, 82724, 76844, 71710, 67189, 63234, 59698, 56551, 53723},
        /* NumJob 16 */
        {77806, 82098, 86794, 92646, 99305, 106335, 114579, 124328, 135477, 119973, 107772, 97838, 89648, 82581, 76757, 71593, 67086, 63149, 59664, 56528, 53730},
        /* NumJob 32 */
        {63005, 67112, 69417, 75543, 81361, 87425, 94452, 107034, 115502, 119553, 107448, 97559, 89307, 82387, 76535, 71425, 66995, 63052, 59626, 56511, 53738}
    },
    /* IO_Depth 16 */
    {
        /* NumJob 1 */
        {50694, 51825, 52574, 54801, 55836, 58835, 60385, 63146, 66168, 67408, 70511, 74807, 73694, 71155, 65595, 65704, 64732, 62440, 58546, 56262, 52807},
        /* NumJob 2 */
        {86238, 91225, 93080, 96988, 98280, 105321, 109798, 111535, 121622, 117249, 106663, 97426, 89506, 82679, 76760, 71647, 67152, 63206, 59696, 56551, 53721},
        /* NumJob 4 */
        {111804, 118124, 124385, 130996, 126719, 148649, 161654, 153685, 134491, 119547, 107562, 97807, 89629, 82685, 76814, 71688, 67177, 63204, 59682, 56532, 53706},
        /* NumJob 8 */
        {108562, 114314, 120789, 127668, 136796, 145499, 156453, 154212, 135115, 119963, 107801, 98000, 89758, 82728, 76863, 71696, 67200, 63205, 59691, 56548, 53724},
        /* NumJob 16 */
        {78259, 82717, 87628, 93107, 100112, 107430, 115509, 127886, 135385, 119957, 107722, 97829, 89616, 82631, 76752, 71565, 67104, 63163, 59667, 56551, 53731},
        /* NumJob 32 */
        {64377, 68089, 70719, 76511, 82375, 88142, 95093, 102668, 113260, 119388, 107304, 97296, 89156, 82154, 76359, 71291, 66899, 62942, 59565, 56479, 53736}
    },
    /* IO_Depth 32 */
    {
        /* NumJob 1 */
        {50660, 51461, 52940, 54181, 56545, 58550, 60747, 62341, 64086, 67276, 71048, 73981, 79479, 74369, 66599, 66795, 66404, 62817, 59415, 56283, 53656},
        /* NumJob 2 */
        {86952, 89653, 93332, 95933, 97068, 102118, 108431, 112606, 119809, 116965, 106379, 97158, 89585, 82681, 76778, 71640, 67154, 63205, 59693, 56552, 53720},
        /* NumJob 4 */
        {111696, 117808, 123780, 131449, 125560, 149465, 161567, 153736, 134475, 119556, 107569, 97812, 89641, 82691, 76811, 71685, 67159, 63196, 59670, 56521, 53690},
        /* NumJob 8 */
        {108407, 114039, 120523, 127643, 136621, 145575, 156262, 154174, 135089, 119962, 107812, 97992, 89753, 82727, 76849, 71684, 67180, 63215, 59696, 56550, 53721},
        /* NumJob 16 */
        {79036, 83435, 88304, 93988, 101183, 108603, 115980, 126051, 135276, 120051, 107709, 97852, 89589, 82581, 76665, 71555, 67065, 63164, 59655, 56536, 53736},
        /* NumJob 32 */
        {64519, 68129, 70888, 76571, 82404, 88105, 95128, 102553, 109537, 119409, 107365, 97397, 89178, 82218, 76447, 71342, 66922, 62984, 59588, 56489, 53735}
    }
};

/* Helper function to get NumJob index for lookup */
static /*inline*/ int get_numjob_index(int numjob) {
    switch (numjob) {
        case 1: return 0;
        case 2: return 1;
        case 4: return 2;
        case 8: return 3;
        case 16: return 4;
        case 32: return 5;
        default: return 0; /* Default to index 0 if not found */
    }
}

/* Helper function to get IO_Depth index for lookup */
static /*inline*/ int get_io_depth_index(int io_depth) {
    switch (io_depth) {
        case 1: return 0;
        case 2: return 1;
        case 4: return 2;
        case 8: return 3;
        case 16: return 4;
        case 32: return 5;
        default: return 0; /* Default to index 0 if not found */
    }
}

/* Helper function to get Split_Ratio index for lookup */
static /*inline*/ int get_split_ratio_index(int split_ratio) {
    /* Index is (100 - split_ratio) / 5 */
    return (100 - split_ratio) / 5;
}

/* Function to lookup bandwidth given IO_Depth, NumJob, and Split_Ratio */
static /*inline*/ int lookup_bandwidth(int io_depth, int numjob, int split_ratio) {
    int io_depth_idx;
    int numjob_idx;
    int split_ratio_idx;
    
    io_depth_idx = get_io_depth_index(io_depth);
    numjob_idx = get_numjob_index(numjob);
    split_ratio_idx = get_split_ratio_index(split_ratio);
    
    /* Bounds checking */
    if (io_depth_idx < 0 || io_depth_idx >= 6 || 
        numjob_idx < 0 || numjob_idx >= 6 ||
        split_ratio_idx < 0 || split_ratio_idx >= 21) {
        return 0; /* Return 0 for out of bounds */
    }
    
    return pmem_nvme_bw_table[io_depth_idx][numjob_idx][split_ratio_idx];
}

/* Only include the calculate_combined_iops function in userspace builds */
#ifndef __KERNEL__
/**
 * Calculate combined IOPS for two devices with a given split ratio
 * 
 * @param device_a_iops IOPS capability of device A
 * @param device_b_iops IOPS capability of device B
 * @param split_ratio Percentage of requests to device A (0-100)
 * @return Combined effective IOPS considering shared queue bottleneck
 */
static /*inline*/ uint64_t calculate_combined_iops(uint64_t device_a_iops, uint64_t device_b_iops, uint64_t split_ratio) {
    uint64_t device_b_ratio;
    uint64_t total_requests_for_a_max = 0;
    uint64_t total_requests_for_b_max = 0;
    uint64_t combined_total_requests;
    
    if (split_ratio > 100) {
        split_ratio = 100;
    }
    
    device_b_ratio = 100 - split_ratio;
    
    /* Calculate total IOPS if each device operated at maximum capacity */
    
    /* Avoid division by zero */
    if (split_ratio > 0) {
        total_requests_for_a_max = (device_a_iops * 100) / split_ratio;
    }
    
    if (device_b_ratio > 0) {
        total_requests_for_b_max = (device_b_iops * 100) / device_b_ratio;
    }
    
    /* The bottleneck is the device that reaches max capacity first */
    
    if (split_ratio == 0) {
        /* All requests go to device B */
        combined_total_requests = device_b_iops;
    } else if (device_b_ratio == 0) {
        /* All requests go to device A */
        combined_total_requests = device_a_iops;
    } else if (total_requests_for_a_max <= total_requests_for_b_max) {
        /* Device A is the bottleneck */
        combined_total_requests = total_requests_for_a_max;
    } else {
        /* Device B is the bottleneck */
        combined_total_requests = total_requests_for_b_max;
    }
    
    return combined_total_requests;
}
#endif /* __KERNEL__ */

#endif /* PMEM_NVME_TABLE_H */ 