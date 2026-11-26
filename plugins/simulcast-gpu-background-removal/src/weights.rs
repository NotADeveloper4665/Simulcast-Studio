// SPDX-FileCopyrightText: 2021 Google LLC
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: Apache-2.0
// Converted from d177d33197099dbeb5ccd430923de39fb941ea19 generated MPLS weights; model onnx-community/mediapipe_selfie_segmentation_landscape@2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3.
pub struct Layer { pub name: &'static str, pub width: u32, pub height: u32, pub bias_width: u32, pub weights: &'static [u16], pub bias: &'static [u16] }
#[path="weights/conv1.rs"] mod conv1;
#[path="weights/conv2_1.rs"] mod conv2_1;
#[path="weights/conv2_2.rs"] mod conv2_2;
#[path="weights/conv3_1.rs"] mod conv3_1;
#[path="weights/conv3_2.rs"] mod conv3_2;
#[path="weights/conv3_3.rs"] mod conv3_3;
#[path="weights/conv4_1.rs"] mod conv4_1;
#[path="weights/conv4_2.rs"] mod conv4_2;
#[path="weights/conv4_3.rs"] mod conv4_3;
#[path="weights/conv5_1.rs"] mod conv5_1;
#[path="weights/conv5_2.rs"] mod conv5_2;
#[path="weights/conv5_3.rs"] mod conv5_3;
#[path="weights/conv6_1.rs"] mod conv6_1;
#[path="weights/conv6_2.rs"] mod conv6_2;
#[path="weights/sub_block1_conv1.rs"] mod sub_block1_conv1;
#[path="weights/sub_block1_conv2.rs"] mod sub_block1_conv2;
#[path="weights/sub_block1_conv3.rs"] mod sub_block1_conv3;
#[path="weights/block1_conv1.rs"] mod block1_conv1;
#[path="weights/block1_conv2.rs"] mod block1_conv2;
#[path="weights/block1_sub_block_conv1.rs"] mod block1_sub_block_conv1;
#[path="weights/block1_sub_block_conv2.rs"] mod block1_sub_block_conv2;
#[path="weights/block1_sub_block_conv3.rs"] mod block1_sub_block_conv3;
#[path="weights/block2_conv1.rs"] mod block2_conv1;
#[path="weights/block2_conv2.rs"] mod block2_conv2;
#[path="weights/block2_sub_block_conv1.rs"] mod block2_sub_block_conv1;
#[path="weights/block2_sub_block_conv2.rs"] mod block2_sub_block_conv2;
#[path="weights/block2_sub_block_conv3.rs"] mod block2_sub_block_conv3;
#[path="weights/block3_conv1.rs"] mod block3_conv1;
#[path="weights/block3_conv2.rs"] mod block3_conv2;
#[path="weights/block3_sub_block_conv1.rs"] mod block3_sub_block_conv1;
#[path="weights/block3_sub_block_conv2.rs"] mod block3_sub_block_conv2;
#[path="weights/block3_sub_block_conv3.rs"] mod block3_sub_block_conv3;
#[path="weights/block4_conv1.rs"] mod block4_conv1;
#[path="weights/block4_conv2.rs"] mod block4_conv2;
#[path="weights/block4_sub_block_conv1.rs"] mod block4_sub_block_conv1;
#[path="weights/block4_sub_block_conv2.rs"] mod block4_sub_block_conv2;
#[path="weights/block4_sub_block_conv3.rs"] mod block4_sub_block_conv3;
#[path="weights/conv7.rs"] mod conv7;
#[path="weights/conv8.rs"] mod conv8;
#[path="weights/final_block1_conv1.rs"] mod final_block1_conv1;
#[path="weights/final_block1_conv2.rs"] mod final_block1_conv2;
#[path="weights/final_block1_conv3.rs"] mod final_block1_conv3;
#[path="weights/final_block1_conv4.rs"] mod final_block1_conv4;
#[path="weights/final_block1_conv5.rs"] mod final_block1_conv5;
#[path="weights/final_block2_conv1.rs"] mod final_block2_conv1;
#[path="weights/final_block2_conv2.rs"] mod final_block2_conv2;
#[path="weights/final_block2_conv3.rs"] mod final_block2_conv3;
#[path="weights/final_block2_conv4.rs"] mod final_block2_conv4;
#[path="weights/final_block2_conv5.rs"] mod final_block2_conv5;
#[path="weights/final_block3_conv1.rs"] mod final_block3_conv1;
#[path="weights/final_block3_conv2.rs"] mod final_block3_conv2;
#[path="weights/final_block3_conv3.rs"] mod final_block3_conv3;
#[path="weights/final_block3_conv4.rs"] mod final_block3_conv4;
#[path="weights/final_block3_conv5.rs"] mod final_block3_conv5;
#[path="weights/conv_transpose.rs"] mod conv_transpose;
pub static LAYERS: &[Layer] = &[
    Layer { name: "conv1", width: 36, height: 4, bias_width: 4, weights: conv1::WEIGHTS, bias: conv1::BIAS },
    Layer { name: "conv2_1", width: 16, height: 4, bias_width: 4, weights: conv2_1::WEIGHTS, bias: conv2_1::BIAS },
    Layer { name: "conv2_2", width: 9, height: 4, bias_width: 4, weights: conv2_2::WEIGHTS, bias: conv2_2::BIAS },
    Layer { name: "conv3_1", width: 16, height: 2, bias_width: 2, weights: conv3_1::WEIGHTS, bias: conv3_1::BIAS },
    Layer { name: "conv3_2", width: 8, height: 4, bias_width: 4, weights: conv3_2::WEIGHTS, bias: conv3_2::BIAS },
    Layer { name: "conv3_3", width: 16, height: 4, bias_width: 4, weights: conv3_3::WEIGHTS, bias: conv3_3::BIAS },
    Layer { name: "conv4_1", width: 16, height: 18, bias_width: 18, weights: conv4_1::WEIGHTS, bias: conv4_1::BIAS },
    Layer { name: "conv4_2", width: 9, height: 18, bias_width: 18, weights: conv4_2::WEIGHTS, bias: conv4_2::BIAS },
    Layer { name: "conv4_3", width: 72, height: 6, bias_width: 6, weights: conv4_3::WEIGHTS, bias: conv4_3::BIAS },
    Layer { name: "conv5_1", width: 24, height: 22, bias_width: 22, weights: conv5_1::WEIGHTS, bias: conv5_1::BIAS },
    Layer { name: "conv5_2", width: 9, height: 22, bias_width: 22, weights: conv5_2::WEIGHTS, bias: conv5_2::BIAS },
    Layer { name: "conv5_3", width: 88, height: 6, bias_width: 6, weights: conv5_3::WEIGHTS, bias: conv5_3::BIAS },
    Layer { name: "conv6_1", width: 24, height: 24, bias_width: 24, weights: conv6_1::WEIGHTS, bias: conv6_1::BIAS },
    Layer { name: "conv6_2", width: 25, height: 24, bias_width: 24, weights: conv6_2::WEIGHTS, bias: conv6_2::BIAS },
    Layer { name: "sub_block1_conv1", width: 96, height: 6, bias_width: 6, weights: sub_block1_conv1::WEIGHTS, bias: sub_block1_conv1::BIAS },
    Layer { name: "sub_block1_conv2", width: 24, height: 24, bias_width: 24, weights: sub_block1_conv2::WEIGHTS, bias: sub_block1_conv2::BIAS },
    Layer { name: "sub_block1_conv3", width: 96, height: 8, bias_width: 8, weights: sub_block1_conv3::WEIGHTS, bias: sub_block1_conv3::BIAS },
    Layer { name: "block1_conv1", width: 32, height: 32, bias_width: 32, weights: block1_conv1::WEIGHTS, bias: block1_conv1::BIAS },
    Layer { name: "block1_conv2", width: 25, height: 32, bias_width: 32, weights: block1_conv2::WEIGHTS, bias: block1_conv2::BIAS },
    Layer { name: "block1_sub_block_conv1", width: 128, height: 8, bias_width: 8, weights: block1_sub_block_conv1::WEIGHTS, bias: block1_sub_block_conv1::BIAS },
    Layer { name: "block1_sub_block_conv2", width: 32, height: 32, bias_width: 32, weights: block1_sub_block_conv2::WEIGHTS, bias: block1_sub_block_conv2::BIAS },
    Layer { name: "block1_sub_block_conv3", width: 128, height: 8, bias_width: 8, weights: block1_sub_block_conv3::WEIGHTS, bias: block1_sub_block_conv3::BIAS },
    Layer { name: "block2_conv1", width: 32, height: 32, bias_width: 32, weights: block2_conv1::WEIGHTS, bias: block2_conv1::BIAS },
    Layer { name: "block2_conv2", width: 25, height: 32, bias_width: 32, weights: block2_conv2::WEIGHTS, bias: block2_conv2::BIAS },
    Layer { name: "block2_sub_block_conv1", width: 128, height: 8, bias_width: 8, weights: block2_sub_block_conv1::WEIGHTS, bias: block2_sub_block_conv1::BIAS },
    Layer { name: "block2_sub_block_conv2", width: 32, height: 32, bias_width: 32, weights: block2_sub_block_conv2::WEIGHTS, bias: block2_sub_block_conv2::BIAS },
    Layer { name: "block2_sub_block_conv3", width: 128, height: 8, bias_width: 8, weights: block2_sub_block_conv3::WEIGHTS, bias: block2_sub_block_conv3::BIAS },
    Layer { name: "block3_conv1", width: 32, height: 24, bias_width: 24, weights: block3_conv1::WEIGHTS, bias: block3_conv1::BIAS },
    Layer { name: "block3_conv2", width: 25, height: 24, bias_width: 24, weights: block3_conv2::WEIGHTS, bias: block3_conv2::BIAS },
    Layer { name: "block3_sub_block_conv1", width: 96, height: 6, bias_width: 6, weights: block3_sub_block_conv1::WEIGHTS, bias: block3_sub_block_conv1::BIAS },
    Layer { name: "block3_sub_block_conv2", width: 24, height: 24, bias_width: 24, weights: block3_sub_block_conv2::WEIGHTS, bias: block3_sub_block_conv2::BIAS },
    Layer { name: "block3_sub_block_conv3", width: 96, height: 8, bias_width: 8, weights: block3_sub_block_conv3::WEIGHTS, bias: block3_sub_block_conv3::BIAS },
    Layer { name: "block4_conv1", width: 32, height: 24, bias_width: 24, weights: block4_conv1::WEIGHTS, bias: block4_conv1::BIAS },
    Layer { name: "block4_conv2", width: 25, height: 24, bias_width: 24, weights: block4_conv2::WEIGHTS, bias: block4_conv2::BIAS },
    Layer { name: "block4_sub_block_conv1", width: 96, height: 6, bias_width: 6, weights: block4_sub_block_conv1::WEIGHTS, bias: block4_sub_block_conv1::BIAS },
    Layer { name: "block4_sub_block_conv2", width: 24, height: 24, bias_width: 24, weights: block4_sub_block_conv2::WEIGHTS, bias: block4_sub_block_conv2::BIAS },
    Layer { name: "block4_sub_block_conv3", width: 96, height: 8, bias_width: 8, weights: block4_sub_block_conv3::WEIGHTS, bias: block4_sub_block_conv3::BIAS },
    Layer { name: "conv7", width: 32, height: 32, bias_width: 32, weights: conv7::WEIGHTS, bias: conv7::BIAS },
    Layer { name: "conv8", width: 32, height: 32, bias_width: 32, weights: conv8::WEIGHTS, bias: conv8::BIAS },
    Layer { name: "final_block1_conv1", width: 128, height: 6, bias_width: 6, weights: final_block1_conv1::WEIGHTS, bias: final_block1_conv1::BIAS },
    Layer { name: "final_block1_conv2", width: 24, height: 6, bias_width: 6, weights: final_block1_conv2::WEIGHTS, bias: final_block1_conv2::BIAS },
    Layer { name: "final_block1_conv3", width: 24, height: 6, bias_width: 6, weights: final_block1_conv3::WEIGHTS, bias: final_block1_conv3::BIAS },
    Layer { name: "final_block1_conv4", width: 24, height: 6, bias_width: 6, weights: final_block1_conv4::WEIGHTS, bias: final_block1_conv4::BIAS },
    Layer { name: "final_block1_conv5", width: 9, height: 6, bias_width: 6, weights: final_block1_conv5::WEIGHTS, bias: final_block1_conv5::BIAS },
    Layer { name: "final_block2_conv1", width: 24, height: 4, bias_width: 4, weights: final_block2_conv1::WEIGHTS, bias: final_block2_conv1::BIAS },
    Layer { name: "final_block2_conv2", width: 16, height: 4, bias_width: 4, weights: final_block2_conv2::WEIGHTS, bias: final_block2_conv2::BIAS },
    Layer { name: "final_block2_conv3", width: 16, height: 4, bias_width: 4, weights: final_block2_conv3::WEIGHTS, bias: final_block2_conv3::BIAS },
    Layer { name: "final_block2_conv4", width: 16, height: 4, bias_width: 4, weights: final_block2_conv4::WEIGHTS, bias: final_block2_conv4::BIAS },
    Layer { name: "final_block2_conv5", width: 9, height: 4, bias_width: 4, weights: final_block2_conv5::WEIGHTS, bias: final_block2_conv5::BIAS },
    Layer { name: "final_block3_conv1", width: 16, height: 4, bias_width: 4, weights: final_block3_conv1::WEIGHTS, bias: final_block3_conv1::BIAS },
    Layer { name: "final_block3_conv2", width: 16, height: 4, bias_width: 4, weights: final_block3_conv2::WEIGHTS, bias: final_block3_conv2::BIAS },
    Layer { name: "final_block3_conv3", width: 16, height: 4, bias_width: 4, weights: final_block3_conv3::WEIGHTS, bias: final_block3_conv3::BIAS },
    Layer { name: "final_block3_conv4", width: 16, height: 4, bias_width: 4, weights: final_block3_conv4::WEIGHTS, bias: final_block3_conv4::BIAS },
    Layer { name: "final_block3_conv5", width: 9, height: 4, bias_width: 4, weights: final_block3_conv5::WEIGHTS, bias: final_block3_conv5::BIAS },
    Layer { name: "conv_transpose", width: 4, height: 4, bias_width: 1, weights: conv_transpose::WEIGHTS, bias: conv_transpose::BIAS },
];
