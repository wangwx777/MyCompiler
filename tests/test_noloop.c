int sum_array(int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) { sum += arr[i]; }
    return sum;
}
int mul_add(int x, int y, int z) {
    return (x + y) * z;
}
