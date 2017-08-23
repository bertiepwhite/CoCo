
import matplotlib.pyplot as plt
import os
import numpy as np
from astropy.table import Table, Column
import astropy.units as u
import pycoco as pcc
import json
import sfdmap
from astropy.io import ascii
import extinction
import shutil
import pandas as pd

SN_dat_folder_OSC = '/Users/bertiepwhite/code/Data/Dat/OSC'
SN_dat_folder_CfA = '/Users/bertiepwhite/code/Data/Dat/CfA'
SN_json_folder = "/Users/bertiepwhite/code/Data/Json/"
SN_txt_folder = "/Users/bertiepwhite/code/Data/Txt/"
Downloads = "/Users/bertiepwhite/Downloads"
Documents = "/Users/bertiepwhite/Documents"
filter_file_location = os.path.join(os.environ['PYCOCO_DATA_DIR'], 'filters/')
spec_file_location = os.path.join(os.environ['PYCOCO_DATA_DIR'], 'spec/')
Lightcurves_folder = os.path.join(os.environ['PYCOCO_DATA_DIR'], 'lc')
recon_folder = os.path.join(os.environ['COCO_ROOT_DIR'], 'recon')
text_file = os.path.join(os.environ['COCO_ROOT_DIR'],'SN Path List.txt')
list_dir = os.path.join(os.environ['COCO_ROOT_DIR'], 'lists')

def SP_comment_spectra(SN_name, spectra_id, bertie = True):
    if bertie == True:
        list_loc = os.path.join(list_dir, (SN_name + '-B.list'))
    else:
        list_loc = os.path.join(list_dir, (SN_name + '.list'))
    with open(list_loc, 'r') as file:
        lines = file.readlines()
        new_lines = []
        for line in lines:
            if spectra_id in line:
                line = '#' + line
                print(line)
            new_lines.append(line)
    with open(list_loc, 'w+') as file:
        for line in new_lines:
            file.write(line)
