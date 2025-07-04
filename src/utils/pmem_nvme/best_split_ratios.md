# Best Split Ratios for Different IO Configurations

This document summarizes the optimal split ratios between PMEM (Device A) and NVMe (Device B) for different IO configurations.

## Summary Table

| IO Depth | NumJob | Best Split Ratio (PMEM:NVMe) | Max Bandwidth |
|----------|--------|------------------------------|---------------|
| 1        | 1      | 100:0                        | 53,984        |
| 1        | 8      | 95:5                         | 116,800       |
| 4        | 4      | 70:30                        | 147,358       |
| 8        | 16     | 60:40                        | 135,477       |
| 16       | 32     | 55:45                        | 119,388       |
| 32       | 16     | 60:40                        | 135,276       |

## Observations

1. **Small IO/Low Concurrency (IO Depth=1, NumJob=1)**
   - Best to use PMEM exclusively (100:0)
   - Bandwidth drops significantly with any NVMe usage

2. **Medium IO/Medium Concurrency (IO Depth=4, NumJob=4)**
   - Optimal split is 70:30
   - This configuration achieves the highest overall bandwidth

3. **High IO/High Concurrency (IO Depth=16+, NumJob=16+)**
   - Optimal split settles around 55:45 to 60:40
   - More balanced workload distribution yields better results

## For IO Depth 32, NumJob 16 (Requested Configuration)

The best split ratio is **60:40** (60% to PMEM, 40% to NVMe) with a bandwidth of 135,276.

### Complete Bandwidth Profile for IO Depth 32, NumJob 16:

```
Split 0:100  -> Bandwidth: 53,736
Split 5:95   -> Bandwidth: 56,536
Split 10:90  -> Bandwidth: 59,655
Split 15:85  -> Bandwidth: 63,164
Split 20:80  -> Bandwidth: 67,065
Split 25:75  -> Bandwidth: 71,555
Split 30:70  -> Bandwidth: 76,665
Split 35:65  -> Bandwidth: 82,581
Split 40:60  -> Bandwidth: 89,589
Split 45:55  -> Bandwidth: 97,852
Split 50:50  -> Bandwidth: 107,709
Split 55:45  -> Bandwidth: 120,051
Split 60:40  -> Bandwidth: 135,276 (Maximum)
Split 65:35  -> Bandwidth: 126,051
Split 70:30  -> Bandwidth: 115,980
Split 75:25  -> Bandwidth: 108,603
Split 80:20  -> Bandwidth: 101,183
Split 85:15  -> Bandwidth: 93,988
Split 90:10  -> Bandwidth: 88,304
Split 95:5   -> Bandwidth: 83,435
Split 100:0  -> Bandwidth: 79,036
```

## Conclusion

The optimal split ratio varies significantly based on IO depth and concurrency level. For high-performance configurations, a more balanced distribution (around 60:40) tends to yield the best results. For systems with IO Depth 32 and NumJob 16, the 60:40 split maximizes bandwidth at 135,276.

Note that these results are based on the provided benchmark data and may vary with different hardware configurations or workload patterns. 