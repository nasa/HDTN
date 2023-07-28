% Kyle Vernyi

%% This file takes two .FLAC audio files as an input and calculates the 
% peak signal to noise ratio (PSNR). The bitdepth of the flac file must be 
% known so that MAXVALUE can be calculated as 
% MAXVALUE = (2^bit depth) - 1
clc

MAXVAL = 2^24 - 1;  % 24 bit audio

transmitted_file="/home/kyle/nasa/dev/test_outputs/test_1/ammonia_trimmed/ammonia_trimmed.flac";
reference_file="/home/kyle/nasa/dev/test_media/official_test_media/ammonia_trimmed.flac";

[y_trans,Fs] = audioread(transmitted_file);
[y_ref,Fs] = audioread(reference_file);

% trim reference to start at beginning of trans
delta_size = length(y_ref) - length(y_trans);
y_ref = y_ref(delta_size + 1:end);

[R, C] = size(y_ref);

delta = y_ref-y_trans;
err   = sum((delta.^2))/(R*C); 
MSE   = sqrt(err);
PSNR  = 20*log10(MAXVAL/MSE);

PSNR
