#include <CL/cl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <fstream>
#include "Util.hpp"
#include "AAlloc.h"
#include "test_vector.h"
using namespace std;



///
//  Attempt to create the program object from a cached binary.  Note that
//  on first run this will fail because the binary has not yet been created.
//
cl_program CreateProgramFromBinary(cl_context context, cl_device_id device, const char* fileName)
{
    FILE *fp = fopen(fileName, "rb");
    if (fp == NULL)
    {
        return NULL;
    }

    // Determine the size of the binary
    size_t binarySize;
    fseek(fp, 0, SEEK_END);
    binarySize = ftell(fp);
    rewind(fp);

    unsigned char *programBinary = new unsigned char[binarySize];
    auto size =  fread(programBinary, 1, binarySize, fp);
    fclose(fp);

    cl_int errNum = 0;
    cl_program program;
    cl_int binaryStatus;

    program = clCreateProgramWithBinary(context,
                                        1,
                                        &device,
                                        &binarySize,
                                        (const unsigned char**)&programBinary,
                                        &binaryStatus,
                                        &errNum);
    delete [] programBinary;
    if (errNum != CL_SUCCESS)
    {
        std::cerr << "Error loading program binary." << std::endl;
        return NULL;
    }

    if (binaryStatus != CL_SUCCESS)
    {
        std::cerr << "Invalid binary for device" << std::endl;
        return NULL;
    }

    errNum = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (errNum != CL_SUCCESS)
    {
        // Determine the reason for the error
        char buildLog[16384];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                              sizeof(buildLog), buildLog, NULL);

        std::cerr << "Error in program: " << std::endl;
        std::cerr << buildLog << std::endl;
        clReleaseProgram(program);
        return NULL;
    }

    return program;
}



int main(int argc, char* argv[])
{

    /*Step1: Getting platforms and choose an available one.*/
    cl_uint numPlatforms;//the NO. of platforms
    cl_platform_id platform;//the chosen platform
    cl_int  status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS)
    {
        std::cout<<"Error: Getting platforms!"<<std::endl;
        return 1;
    }

    /*For clarity, choose the first available platform. */
    if(numPlatforms > 0)
    {
        cl_platform_id* platforms = (cl_platform_id*)malloc(numPlatforms* sizeof(cl_platform_id));
        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        platform = platforms[0];
        free(platforms);
    }

    /*Step 2:Query the platform and choose the first CPU device if has one.
     *Otherwise use the second CPU  device which should be intel.*/
    cl_uint             numDevices = 0;
    cl_device_id        *devices;
    status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 0, NULL, &numDevices);
    std::cout << "Choose CPU as default device."<<std::endl;
    status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 0, NULL, &numDevices);
    devices = (cl_device_id*)malloc(numDevices * sizeof(cl_device_id));

    status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, numDevices, devices, NULL);


    /*Step 3: Create context.*/
    cl_context context = clCreateContext(NULL,1, devices,NULL,NULL,NULL);

    /*Step 4: Creating command queue associate with the context.*/
    cl_command_queue commandQueue = clCreateCommandQueue(context, devices[0], CL_QUEUE_PROFILING_ENABLE, NULL);

    cl_program program = CreateProgramFromBinary(context, devices[0], "kernel.ir");
    if (program == NULL)
    {
        std::cout << "Binary not loaded, create from source..." << std::endl;
        exit(1);
  
    }
    /*Step 7: Initial input,output for the host and create memory objects for the kernel*/


    auto  num  = 8;
    std::vector<cl_int, AAlloc::AlignedAllocator<cl_int, 128> > Keys_in;
    std::vector<cl_int, AAlloc::AlignedAllocator<cl_int, 128> > Keys_out(num);
    //Keys_in.resize(num_in);
    Keys_in.assign(A, A+num);
    for(auto i:Keys_in)
        printf("%d\n",i);




    cl_int* input =  Keys_in.data();
    cl_int* output = Keys_out.data();

    std::vector<cl_event> events_write_buffer(1);
    std::vector<cl_event> events_read_buffer(1);

    cl_mem inputBuffer = clCreateBuffer(context,
                                        CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                        (size_t)(num) * sizeof(cl_int),
                                        input,
                                        &status);
   if (status != CL_SUCCESS)
   {
       std::cout<<"Error: inputBuffer!"<<std::endl;
       std::cout << get_error_string(status)  <<std::endl;
       return 1;
    }


    cl_mem outputBuffer = clCreateBuffer(context,
    CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
    (size_t)(num) * sizeof(cl_int),
    output,
    &status);

   if (status != CL_SUCCESS)
   {
    std::cout<<"Error: outputBuffer!"<<std::endl;
    std::cout << get_error_string(status)  <<std::endl;
    return 1;
   }




    status = clEnqueueWriteBuffer (commandQueue,
                                    inputBuffer,
                                    CL_FALSE,
                                    0,
                                    (size_t)num * sizeof(cl_int),
                                    (void *)input,
                                    0,
                                    NULL,
                                    &events_write_buffer[0]);

    if (status != CL_SUCCESS)
    {
    std::cout<<"Error: clEnqueueWriteBuffer!"<<std::endl;
    std::cout << get_error_string(status)  <<std::endl;
        return 1;
    }


    /*Step 8: Create kernel object */
    cl_kernel kernel = clCreateKernel(program,"mips_main", &status);
    if (status != CL_SUCCESS)
    {
        std::cout<<"Error: creating kernels! : ";
        std::cout << get_error_string(status)  <<std::endl;
        return 1;
    }



    /*Step 9: Sets Kernel arguments.*/
    status = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&inputBuffer);
    if (status != CL_SUCCESS)
    {
        std::cout<<"Error: setting up kernel argument no 1!"<<std::endl;
        return 1;
    }

    status = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&outputBuffer);
    if (status != CL_SUCCESS)
    {
        std::cout<<"Error: setting up kernel argument no 1!"<<std::endl;
        return 1;
    }

    std::cout<<"Lunching sha kernel!"<<std::endl;

    /*Step 10: Running the kernel.*/
    cl_event kernel_exec_event;
    status = clEnqueueTask( commandQueue,
                            kernel,
                            events_write_buffer.size(),
                            events_write_buffer.data(),
                            &kernel_exec_event);
    if (status != CL_SUCCESS)
    {
        std::cout<<"Error: seting up clEnqueueNDRangeKernel!"<<std::endl;
        std::cout << get_error_string(status)  <<std::endl;
        return 1;
    }

    /*Step 11: Read the std::cout put back to host memory.*/


    status = clEnqueueReadBuffer (commandQueue,
        outputBuffer,
        CL_TRUE,
        0,
        (size_t)num * sizeof(cl_int),
        (void *)output,
        1,
        &kernel_exec_event,
        &events_read_buffer[0]);

    if (status != CL_SUCCESS)
    {
        std::cout<<"Error: clEnqueueReadBuffer!"<<std::endl;
        std::cout << get_error_string(status)  <<std::endl;
        return 1;
    }

    //std::cout<<"\n\noutput data:"<<std::endl;

/*    for (cl_int i : Keys_out )
    {
        std::cout <<  i << "\n";

    }*/
    clWaitForEvents(1, &events_read_buffer[0]);

    std::cout<<"verifying mips kernel results!"<<std::endl;

    for (int i  = 0; i < 8; i++)
    {
        if (Keys_out[i] != outData[i])
        {
            std::cout<<"mips failed"<<std::endl;
            std::cout << Keys_out[i] << " i " << i << "\n";
            break;
        }

    }



    cl_ulong start = 0, end = 0;

    double  pcie_time= 0;

    for (unsigned i = 0; i < events_write_buffer.size(); ++i)
    {
         clGetEventProfilingInfo(events_write_buffer[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
         clGetEventProfilingInfo(events_write_buffer[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
         pcie_time += (cl_double)(end - start)*(cl_double)(1e-06);

    }


    for (unsigned i = 0; i < events_read_buffer.size(); ++i)
    {
         clGetEventProfilingInfo(events_read_buffer[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
         clGetEventProfilingInfo(events_read_buffer[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
         pcie_time += (cl_double)(end - start)*(cl_double)(1e-06);

    }
    clGetEventProfilingInfo(kernel_exec_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(kernel_exec_event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
     //END-START gives you hints on kind of “pure HW execution time”
     //the resolution of the events is 1e-09 sec
    auto g_NDRangePureExecTimeMs = (cl_double)(end - start)*(cl_double)(1e-06);

    std::cout<<"\n\nKernel Execution Time: "<< g_NDRangePureExecTimeMs << " ms\n"
             << "PCIE Transfer Time: "<< pcie_time << " ms\n"
             << "Total ExecutionTime: "<< g_NDRangePureExecTimeMs +  pcie_time << " ms"
             <<std::endl;


    /*Step 12: Clean the resources.*/
    status = clReleaseKernel(kernel);//*Release kernel.
    //status = clReleaseKernel(kernel2);
    status = clReleaseProgram(program); //Release the program object.
    status = clReleaseMemObject(inputBuffer);//Release mem object.
    //status = clReleaseMemObject(inputBuffer2);
    status = clReleaseMemObject(outputBuffer);
    //status = clReleaseMemObject(outputBuffer2);
    status = clReleaseCommandQueue(commandQueue);//Release  Command queue.
    status = clReleaseContext(context);//Release context.

    if (devices != NULL)
    {
        free(devices);
        devices = NULL;
    }

    return 0;
}
