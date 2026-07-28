#!/usr/bin/env bash
source ~/miniconda3/etc/profile.d/conda.sh
conda activate ks
python main.py
# python manage.py runserver 0.0.0.0:6001 2>&1 | tee run.log