#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>

using namespace cv;

// Function to load and preprocess the image
std::vector<float> preprocessImage(const std::string& image_path, const std::vector<int64_t>& input_shape) {
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        throw std::runtime_error("Could not read the image: " + image_path);
    }

    cv::Mat resized_image;
    cv::resize(image, resized_image, cv::Size(input_shape[2], input_shape[3]));

    resized_image.convertTo(resized_image, CV_32F, 1.0 / 255);

    std::vector<float> input_tensor_values(input_shape[1] * input_shape[2] * input_shape[3]);
    std::memcpy(input_tensor_values.data(), resized_image.data, input_tensor_values.size() * sizeof(float));

    return input_tensor_values;
}

// Function to load the ONNX model and create a session
Ort::Session loadModel(Ort::Env& env, const std::string& model_path, Ort::SessionOptions& session_options) {
    return Ort::Session(env, model_path.c_str(), session_options);
}

// Function to get the input name of the model
std::string getInputName(Ort::Session& session, Ort::AllocatorWithDefaultOptions& allocator) {
    Ort::AllocatedStringPtr name_allocator = session.GetInputNameAllocated(0, allocator);
    return std::string(name_allocator.get());
}

// Function to get the output name of the model
std::string getOutputName(Ort::Session& session, Ort::AllocatorWithDefaultOptions& allocator) {
    Ort::AllocatedStringPtr name_allocator = session.GetOutputNameAllocated(0, allocator);
    return std::string(name_allocator.get());
}

// Function to post-process the results (for simplicity, just prints the output)
void postprocessResults(const std::vector<float>& results) {
    std::cout << "Output tensor size: " << results.size() << std::endl;
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << results[i] << " ";
    }
    std::cout << std::endl;
}

// Function to run inference on the model
std::vector<float> runInference(Ort::Session& session, const std::vector<float>& input_tensor_values, const std::vector<int64_t>& input_shape) {
    Ort::AllocatorWithDefaultOptions allocator;

    std::string input_name = getInputName(session, allocator);
    std::string output_name = getOutputName(session, allocator);

    const char* input_name_ptr = input_name.c_str();
    const char* output_name_ptr = output_name.c_str();

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, const_cast<float*>(input_tensor_values.data()), input_tensor_values.size(), input_shape.data(), input_shape.size());

    auto output_tensors = session.Run(Ort::RunOptions{nullptr}, &input_name_ptr, &input_tensor, 1, &output_name_ptr, 1);

    float* floatarr = output_tensors[0].GetTensorMutableData<float>();
    size_t output_tensor_size = output_tensors[0].GetTensorTypeAndShapeInfo().GetElementCount();

    return std::vector<float>(floatarr, floatarr + output_tensor_size);
}


int main(int argc, char* argv[])
{

    // Check for the correct number of arguments
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <image_path>" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::string image_path = argv[2];

    // Initialize ONNX Runtime environment
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "ONNXRuntime");

    // Create session options
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

    try {
        // Load model
        Ort::Session session = loadModel(env, model_path, session_options);

        // Define input shape (e.g., {1, 3, 416, 416})
        std::vector<int64_t> input_shape = {1, 3, 640, 640};

        // Preprocess image
        std::vector<float> input_tensor_values = preprocessImage(image_path, input_shape);

        // Run inference
        std::vector<float> results = runInference(session, input_tensor_values, input_shape);

        // Postprocess results
        postprocessResults(results);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}