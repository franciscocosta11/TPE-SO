/^player.*pipe/ {
        countbypid[$2]++
}
END {
        for (pid in countbypid) {
                printf "%d: %d\n", pid, countbypid[pid]
        }
}