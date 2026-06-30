int sum_array(int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) { sum += arr[i]; }
    return sum;
}
int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) { result = result * i; }
    return result;
}
