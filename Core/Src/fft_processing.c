#include "fft_processing.h"
#include "stdio.h"
// FFT变量
static float32_t fft_input[FFT_SIZE];
static float32_t fft_output[FFT_SIZE];
static float32_t magnitude[FFT_SIZE/ 2];
static arm_rfft_fast_instance_f32 fft_instance;


void FFT_Init(void)
{
    // 初始化FFT实例
    arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);
}

const float32_t *FFT_Get_Magnitude(void)
{
    return magnitude;
}

void Process_Data(int32_t *data)
{
    for(int i = 0; i < FFT_SIZE; i++)
    {
        // 转换为浮点数（-1.0 到 1.0）
        fft_input[i] = (float32_t)data[i] / 8388608.0f; // 24位有符号数

        // 应用汉宁窗减少频谱泄漏
        fft_input[i] *= (0.5f * (1.0f - arm_cos_f32(2 * PI * i / (FFT_SIZE - 1))));
    }

    // 执行FFT
    arm_rfft_fast_f32(&fft_instance, fft_input, fft_output, 0);
}

void Analyze_Frequency_Spectrum(float32_t *magnitude)
{
    // 频率分辨率
    float freq_resolution = (float)SAMPLE_RATE / FFT_SIZE;
    
    // 寻找主要频率成分
    float32_t max_magnitude = 0;
    uint32_t max_bin = 0;
    
    arm_max_f32(magnitude, FFT_SIZE/2, &max_magnitude, &max_bin);
    
    float dominant_freq = max_bin * freq_resolution;
    
    // 计算各频段能量（示例：低频、中频、高频）
    float low_freq_energy = 0, mid_freq_energy = 0, high_freq_energy = 0;
    
    for(int i = 0; i < FFT_SIZE/2; i++)
    {
        float freq = i * freq_resolution;
        if(freq < 300)        // 低频
            low_freq_energy += magnitude[i];
        else if(freq < 3000)  // 中频
            mid_freq_energy += magnitude[i];
        else                  // 高频
            high_freq_energy += magnitude[i];
    }
    
    // 输出结果或进一步处理
    printf("主频率: %.1f Hz, 能量分布: 低=%.2f, 中=%.2f, 高=%.2f\n", dominant_freq, low_freq_energy, mid_freq_energy, high_freq_energy);
}

// 生成测试正弦波验证FFT
void Generate_Test_Signal(float32_t *buffer, float freq, uint32_t size)
{
    for(int i = 0; i < size; i++)
    {
        buffer[i] = arm_sin_f32(2 * PI * freq * i / SAMPLE_RATE);
    }
}

void Test_FFT_With_Signal(float test_freq)
{
    printf("===FFT测试开始===\n");
    printf("测试频率: %.1f Hz\n", test_freq);
    printf("采样率: %d Hz, FFT点数: %d\n", SAMPLE_RATE, FFT_SIZE);
    printf("频率分辨率: %.2f Hz\n", (float)SAMPLE_RATE / FFT_SIZE);

    // 生成测试信号
    float32_t test_signal[FFT_SIZE];
    Generate_Test_Signal(test_signal, test_freq, FFT_SIZE);

    // 转换为int32_t格式模拟真实数据
    int32_t simulated_adc_data[FFT_SIZE];
    for(int i = 0; i < FFT_SIZE; i++)
    {
        // 模拟24位ADC数据, 除以2^23，因为有符号
        simulated_adc_data[i] = (int32_t)(test_signal[i] * 8388607.0f);
    }

    // 处理数据
    Process_Data(simulated_adc_data);
    
    for(int i = 0; i < FFT_SIZE / 2; i++)
    {
        float32_t real = fft_output[2 * i];
        float32_t imag = fft_output[2 * i + 1];
        magnitude[i]  = sqrtf(real * real + imag * imag);
    }

    // 分析频谱
    Analyze_Frequency_Spectrum(magnitude);

    // 额外调试信息, 显示峰值附近的bin
    float freq_resolution = (float) SAMPLE_RATE / FFT_SIZE;
    uint32_t expected_bin = (uint32_t)(test_freq / freq_resolution);

    printf("预期峰值在 bin %lu (%.1f Hz)\n", expected_bin, expected_bin * freq_resolution);
    printf("峰值附近的值:\n");

    // 显示预期峰值前后前5个bin
    for(int i = (int)expected_bin -5; i <= (int)expected_bin + 5; i++)
    {
        if(i >= 0 && i < FFT_SIZE/ 2)
        {
            printf("Bin %3d: %6.1f Hz -> 幅度: %8.4f\n", i, i * freq_resolution, magnitude[i]);
        }
    }

    printf("===FFT测试结束===\n\n");

}

// 多频率测试
void Test_FFT_Multiple_Frequencies(void)
{
    printf("正在进行多频率FFT测试...\n\n");

    // 测试不同频率
    Test_FFT_With_Signal(1000.0f);
    HAL_Delay(1000);
    Test_FFT_With_Signal(2000.0f);
    HAL_Delay(1000);
    Test_FFT_With_Signal(5000.0f);
    HAL_Delay(1000);

    // 测试边界频率
    Test_FFT_With_Signal(100.0f);
    HAL_Delay(1000);
    Test_FFT_With_Signal(15000.0f);
}

/**
 * @brief 计算FFT结果的幅值(幅度谱)
 */
void compute_fft_magnitude(float32_t *fft_data, float32_t *magnitude)
{
    for(int i = 0; i < FFT_SIZE / 2; i++)
    {
        float32_t real = fft_output[2 * i];
        float32_t imag = fft_output[2 * i + 1];
        magnitude[i]  = sqrtf(real * real + imag * imag);
    }
}
















