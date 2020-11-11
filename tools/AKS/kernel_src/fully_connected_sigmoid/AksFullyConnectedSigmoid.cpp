/*
 * Copyright 2019 Xilinx Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

#include <cblas.h>

#include <assert.h>
#include <math.h>
#include <queue>
#include <fstream>

#include <aks/AksKernelBase.h>
#include <aks/AksDataDescriptor.h>
#include <aks/AksNodeParams.h>
#include <aks/AksLogger.h>

#include <H5Cpp.h>
using namespace std;
using namespace AKS;


struct weightsDescriptor {
  std::string _weigthsFile;
  std::vector<float> _weights;
  std::vector<float> _bias;
};

class FullyConnectedSigmoid : public AKS::KernelBase
{
  public:
    FullyConnectedSigmoid () {}
    void nodeInit (AKS::NodeParams*);
    int exec_async (
           std::vector<AKS::DataDescriptor*> &in, 
           std::vector<AKS::DataDescriptor*> &out, 
           AKS::NodeParams* nodeParams,
           AKS::DynamicParamValues* dynParams);

    std::map<AKS::NodeParams*, weightsDescriptor> _wbParams;
};

extern "C" { /// Add this to make this available for python bindings and 

void computeFC(float *weight, float *bias, float *data,
    int M, int N, int K, float *output)
// inBatch, outSize, inSize
{
  int lda = K;
  int ldb = K;
  for (int batch=0; batch<M; batch++) {
    float* to_fill = output + (batch*N);
    cblas_sgemv(CblasRowMajor,
        (CBLAS_TRANSPOSE)CblasNoTrans,
        N, K, 1.,
        weight, K, data, 1, 0., to_fill, 1);
  }
  lda = 1;
  ldb = N;
  std::vector<float> bias_multiplier(M, 1.);
  for (int batch=0; batch<M; batch++) {
    float* to_fill = output + (batch*N);
    cblas_sgemv(
        CblasRowMajor,
        (CBLAS_TRANSPOSE)CblasNoTrans,
        N, 1, 1., bias, 1,
        &(bias_multiplier[0]), 1, 1., to_fill, 1);
  }
}

AKS::KernelBase* getKernel (AKS::NodeParams *params)
{
  //std::string weightsFile = params->_stringParams["vitis_rundir"] + "/weights.h5";
  return new FullyConnectedSigmoid();
}

}//extern "C"

void FullyConnectedSigmoid::nodeInit (AKS::NodeParams* params)
{
  struct weightsDescriptor wbDesc;
  wbDesc._weigthsFile = params->getValue<string>("weights");

  H5::Exception::dontPrint(); // Don't print H5 exceptions, we will handle

  const H5std_string FILE_NAME(wbDesc._weigthsFile);  // Open up weights
  H5::H5File file(FILE_NAME, H5F_ACC_RDONLY);  //   as hdf5 file

  /// Load FC Weights
  {
      // Open the HDF5 Dataset for given layer
      std::string dsname = params->getValue<string>("fc_weights");
      const H5std_string DATASET_NAME(dsname);
      H5::DataSet dataset = file.openDataSet(DATASET_NAME);

      // Get the dataspace // Defines number of dimensions, and the size of each dimension
      H5::DataSpace dataspace = dataset.getSpace();

      // Get the number of dimensions in the dataspace.
      int rank = dataspace.getSimpleExtentNdims();

      // Get the dimension size of each dimension in the dataspace
      std::vector<hsize_t> dims(rank);
      int ndims = dataspace.getSimpleExtentDims(&dims[0], NULL);

      // Get the layerName from the dataset's layer_name attribute
      const H5std_string  ATTR_NAME("layer_name");
      H5::Attribute attribute = dataset.openAttribute(ATTR_NAME);
      H5::StrType stype = attribute.getStrType();
      std::string layerName;
      attribute.read(stype,layerName);

      if (1)
      {
        SLOG(LogLevel::DEBUG, 
            _DEBUG << "Loading HDF5 dataset: " << DATASET_NAME << ", from file: " 
            << FILE_NAME << ",layerName: " << layerName << ", having dataspace:" << std::endl;
            _DEBUG << "rank: " << rank << ", dimensions: ";
            for (int i=0;i<dims.size();i++)
            std::cout << (unsigned long)(dims[i]) << " x ";
            std::cout << std::endl;);
      }

      // Get the raw data
      std::vector<float> weights;
      int space = 1;
      for (int i=0;i<rank;i++)
        space *= dims[i];
      weights.resize(space);
      dataset.read(&weights[0],H5::PredType::NATIVE_FLOAT,dataspace,dataspace);

      // Uncomment below if you don't trust hdf5 weights, and you need to see them
      //std::ofstream outfile;
      //outfile.open("outfile.txt." + std::to_string(fi));
      //for (int i=0;i<space;i++)
      //  outfile << std::fixed << weights[i] << " ";

      //wbDesc._weights[layerName] = weights;
      wbDesc._weights = weights;
  }

  /// Load FC Bias
  {
      // Open the HDF5 Dataset for given layer
      std::string dsname = params->getValue<string>("fc_bias");
      const H5std_string DATASET_NAME(dsname);
      H5::DataSet dataset = file.openDataSet(DATASET_NAME);

      // Get the dataspace // Defines number of dimensions, and the size of each dimension
      H5::DataSpace dataspace = dataset.getSpace();

      // Get the number of dimensions in the dataspace.
      int rank = dataspace.getSimpleExtentNdims();

      // Get the dimension size of each dimension in the dataspace
      std::vector<hsize_t> dims(rank);
      int ndims = dataspace.getSimpleExtentDims(&dims[0], NULL);

      // Get the layerName from the dataset's layer_name attribute
      const H5std_string  ATTR_NAME("layer_name");
      H5::Attribute attribute = dataset.openAttribute(ATTR_NAME);
      H5::StrType stype = attribute.getStrType();
      std::string layerName;
      attribute.read(stype,layerName);

      if (1)
      {
        SLOG(LogLevel::DEBUG, 
            _DEBUG << "Loading HDF5 dataset: " << DATASET_NAME << ", from file: " 
                << FILE_NAME << ",layerName: " << layerName << ", having dataspace:" << std::endl;
            _DEBUG << "rank: " << rank << ", dimensions: ";
            for (int i=0;i<dims.size();i++)
                std::cout << (unsigned long)(dims[i]) << " x ";
            std::cout << std::endl;)
      }

      // Get the raw data
      std::vector<float> bias;
      int space = 1;
      for (int i=0;i<rank;i++)
        space *= dims[i];
      bias.resize(space);
      dataset.read(&bias[0],H5::PredType::NATIVE_FLOAT,dataspace,dataspace);

      //wbDesc._bias[layerName] = bias;
      wbDesc._bias = bias;
  }
  _wbParams[params] = std::move(wbDesc);
  /// DEBUG
  /*
  for (const auto& w: _wbParams[params]._weights)
    std::cout << w.first << std::endl;
  for (const auto& b: _wbParams[params]._bias)
    std::cout << b.first << std::endl;
  */
}


float CPUCalcSigmoid(float data) {
  return 1.0 / (1 + exp(-1 * data));
}


void convert_NCHW_to_NHWC(float* inData, int inBatch, int inChannel,
                          int inHeight, int inWidth, float* outData) {
  for (int i=0; i<inBatch; i++) {
    for (int c=0; c<inChannel; c++) {
      for (int h=0; h<inHeight; h++) {
        for (int w=0; w<inWidth; w++) {
          outData[i*(inChannel*inHeight*inWidth) + h*(inWidth*inChannel) + w*(inChannel) + c] = \
            inData[i*(inChannel*inHeight*inWidth) + c*(inHeight*inWidth) + h*(inWidth) + w];
        }
      }
    }
  }
}


int FullyConnectedSigmoid::exec_async (
           std::vector<AKS::DataDescriptor*> &in, 
           std::vector<AKS::DataDescriptor*> &out, 
           AKS::NodeParams* nodeParams,
           AKS::DynamicParamValues* dynParams)
{
  int transpose_input = \
    nodeParams->_intParams.find("transpose_input") == nodeParams->_intParams.end() ?
      0 : nodeParams->_intParams["transpose_input"];

  /// Get input and output data shapes
  std::vector <int> inShape  = in[0]->getShape();

  int inBatch  = inShape[0];
  int inChannel= inShape[1];
  int inHeight = inShape[2];
  int inWidth  = inShape[3];
  float *inData = (float*) in[0]->data();

  float* newInData = inData;
  if (transpose_input) {
    newInData = new float[inBatch*inChannel*inHeight*inWidth];
    convert_NCHW_to_NHWC(inData, inBatch, inChannel, inHeight, inWidth, newInData);
  }

  int outSize = _wbParams[nodeParams]._bias.size();

  // std::cout << "[DBG] FullyConnectedSigmoid: inHeight x inWidth  : " << inHeight << " x " << inWidth << std::endl;
  // std::cout << "[DBG] FullyConnectedSigmoid: inBatch x inchannel : " << inBatch << " x " << inChannel << std::endl;
  // std::cout << "[DBG] FullyConnectedSigmoid: biasSize : " << outSize << std::endl;

  float *fcOutPtr = new float[inBatch, outSize];
  computeFC((float*)(&_wbParams[nodeParams]._weights[0]), 
            (float*)(&_wbParams[nodeParams]._bias[0]),
            newInData, inBatch, outSize, inChannel*inHeight*inWidth, fcOutPtr);

  AKS::DataDescriptor *outDD = new AKS::DataDescriptor ({1, 1, 1, 1}, AKS::DataType::FLOAT32);
  float *outData = static_cast<float*>(outDD->data());
  outData[0] = CPUCalcSigmoid(fcOutPtr[0]);

  delete[] fcOutPtr;
  out.push_back(outDD);
  return -1; /// No wait
}

