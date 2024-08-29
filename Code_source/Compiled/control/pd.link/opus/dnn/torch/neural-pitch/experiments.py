"""
Running the experiments;
    1. RCA vs SNR for our models, CREPE, LPCNet
"""

import argparse
parser = argparse.ArgumentParser()

parser.add_argument('ptdb_root', type=str, help='Root Directory for PTDB generated by running ptdb_process.sh ')
parser.add_argument('output', type=str, help='Output dump file name')
parser.add_argument('method', type=str, help='Output Directory to save experiment dumps',choices=['model','lpcnet','crepe'])
parser.add_argument('--noise_dataset', type=str, help='Location of the Demand Datset',default = './',required=False)
parser.add_argument('--noise_type', type=str, help='Type of additive noise',default = 'synthetic',choices=['synthetic','demand'],required=False)
parser.add_argument('--pth_file', type=str, help='.pth file to analyze',default = './',required = False)
parser.add_argument('--fraction_files_analyze', type=float, help='Fraction of PTDB dataset to test on',default = 1,required = False)
parser.add_argument('--threshold_rca', type=float, help='Cent threshold when computing RCA',default = 50,required = False)
parser.add_argument('--gpu_index', type=int, help='GPU index to use if multiple GPUs',default = 0,required = False)

args = parser.parse_args()

import os
os.environ["CUDA_DEVICE_ORDER"] = "PCI_BUS_ID"
os.environ["CUDA_VISIBLE_DEVICES"] = str(args.gpu_index)

import json
from evaluation import cycle_eval

if args.method == 'model':
    dict_store = cycle_eval([args.pth_file], noise_type = args.noise_type, noise_dataset = args.noise_dataset, list_snr = [-20,-15,-10,-5,0,5,10,15,20], ptdb_dataset_path = args.ptdb_root,fraction = args.fraction_files_analyze,thresh = args.threshold_rca)
else:
    dict_store = cycle_eval([args.method], noise_type = args.noise_type, noise_dataset = args.noise_dataset, list_snr = [-20,-15,-10,-5,0,5,10,15,20], ptdb_dataset_path = args.ptdb_root,fraction = args.fraction_files_analyze,thresh = args.threshold_rca)

dict_store["method"] = args.method
if args.method == 'model':
    dict_store['pth'] = args.pth_file

with open(args.output, 'w') as fp:
    json.dump(dict_store, fp)