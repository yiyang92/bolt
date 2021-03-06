// Copyright (C) 2019. Huawei Technologies Co., Ltd. All rights reserved.

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "type.h"
#include "tensor_desc.h"
#include "error.h"
#include "tensor_computing_type.h"
#include "cpu/general/tensor_computing_general.h"


template<typename T>
EE priorbox(T* output,
            U32 ih_layer, U32 iw_layer, U32 ih_img, U32 iw_img, 
            std::vector<F32> minsizes, std::vector<F32> maxsizes, std::vector<F32> ars, U32 flip, U32 clip, F32* vars, I32 imageW, I32 imageH,
            F32 stepW, F32 stepH, 
            F32 offset)
{
    U32 layer_w = iw_layer;
    U32 layer_h = ih_layer;

    int img_w, img_h;
    if(imageH == 0 || imageW == 0){
        img_w = iw_img;
        img_h = ih_img;  
    } else {
        img_w = imageW;
        img_h = imageH;
    }
    F32 stp_h, stp_w;
    if (stepW == 0 || stepH == 0){
        stp_w = static_cast<F32>(ceil((img_w)/layer_w));
        stp_h = static_cast<F32>(ceil((img_h)/layer_h));
    } else{
        stp_w = stepW;
        stp_h = stepH;
    }

    U32 num_priorboxs = ars.size();
    if(flip){
        num_priorboxs = num_priorboxs * 2;
    }
    U32 num_minsize = minsizes.size();
    num_priorboxs = (num_priorboxs + 1) * num_minsize;
    if(!maxsizes.empty()){
        U32 num_maxsize = maxsizes.size();
        num_priorboxs = num_priorboxs + num_maxsize;
    }
    int dim = layer_h * layer_w * num_priorboxs * 4;
    int idx = 0;
    for (U32 h = 0 ; h < layer_h ; h++){
       for (U32 w = 0 ; w < layer_w ; w++){
            F32 center_x = (w + offset) * stp_w;
            F32 center_y = (h + offset) * stp_h;
            F32 box_w , box_h;
            for( int n = 0 ; n < (int)minsizes.size() ; n++){
                F32 minsize = minsizes[n];
                box_w = box_h = minsize;
                output[idx++] = (center_x - box_w/2) / img_w;
                output[idx++] = (center_y - box_h/2) / img_h;
                output[idx++] = (center_x + box_w/2) / img_w;
                output[idx++] = (center_y + box_h/2) / img_h;
            
                if ((int)maxsizes.size() > 0) {
                    F32 maxsize = maxsizes[n];
                    box_w = box_h = sqrt(minsize * maxsize);
                    output[idx++] = (center_x - box_w/2) / img_w;
                    output[idx++] = (center_y - box_h/2) / img_h;
                    output[idx++] = (center_x + box_w/2) / img_w;
                    output[idx++] = (center_y + box_h/2) / img_h;
                }

                for (int a = 0; a < (int)ars.size(); a++){
                    F32 ar = ars[a];
                    box_w = minsize * sqrt(ar);
                    box_h = minsize / sqrt(ar);
                    output[idx++] = (center_x - box_w/2) / img_w;
                    output[idx++] = (center_y - box_h/2) / img_h;
                    output[idx++] = (center_x + box_w/2) / img_w;
                    output[idx++] = (center_y + box_h/2) / img_h;
                    if(flip){
                        output[idx++] = (center_x - box_h/2) / img_w;
                        output[idx++] = (center_y - box_w/2) / img_h;
                        output[idx++] = (center_x + box_h/2) / img_w;
                        output[idx++] = (center_y + box_w/2) / img_h;
                    }
                }            
            }
        }
    }

    if (clip) {
        for (int i = 0; i < dim; i++) {
            output[i] = std::min<T>(std::max<T>(output[i], 0.), 1.);
        }
    }

    for(int i = 0 ; i < dim/4 ; i++){
        output[idx++] = vars[0];
        output[idx++] = vars[1]; 
        output[idx++] = vars[2]; 
        output[idx++] = vars[3]; 
    }
    return SUCCESS;
}

EE priorbox_general(std::vector<TensorDesc> inputDesc, PriorBoxDesc priorboxDesc, TensorDesc outputDesc, void* output)
{   
    UNUSED(outputDesc);
    if (nullptr == output) {
        CHECK_STATUS(NULL_POINTER);
    }
    U32 num = inputDesc.size();
    if (num != 2) return NOT_MATCH;
    DataType idt0, idt1;
    DataFormat idf0, idf1;
    U32 in0 = 0, ic0 = 0, ih0 = 0, iw0 = 0;
    U32 in1 = 0, ic1 = 0, ih1 = 0, iw1 = 0;
    CHECK_STATUS(tensor4dGet(inputDesc[0], &idt0, &idf0, &in0, &ic0, &ih0, &iw0));
    CHECK_STATUS(tensor4dGet(inputDesc[1], &idt1, &idf1, &in1, &ic1, &ih1, &iw1));

    std::vector<F32> minsizes = priorboxDesc.min_sizes;
    std::vector<F32> maxsizes = priorboxDesc.max_sizes;
    std::vector<F32> ars = priorboxDesc.aspect_ratios;
    U32 flip = priorboxDesc.flip;
    U32 clip = priorboxDesc.clip;
    F32 vars[4];
    for (int i = 0; i < 4 ; i++){
        vars[i] = priorboxDesc.variances[i];
    }
    U32 imageH = priorboxDesc.image_h;
    U32 imageW = priorboxDesc.image_w;
    F32 stepH = priorboxDesc.step_h;
    F32 stepW = priorboxDesc.step_w;
    F32 offset = priorboxDesc.offset;

    EE ret = SUCCESS; 
    switch (idt0) {
#ifdef _USE_FP32
        case DT_F32:
            ret = priorbox((F32*)output,
                            ih0, iw0, ih1, iw1,
                            minsizes, maxsizes, ars, flip, clip, vars, imageW, imageH,
                            stepW, stepH, 
                            offset);
            break;
#endif
#ifdef _USE_FP16
        case DT_F16:
            ret = priorbox((F16*)output,
                            ih0, iw0, ih1, iw1,
                            minsizes, maxsizes, ars, flip, clip, vars, imageW, imageH,
                            stepW, stepH, 
                            offset);
            break;
#endif
        default:
            ret = NOT_SUPPORTED;
    }       
    return ret;
}