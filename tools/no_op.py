import argparse
import time

parser = argparse.ArgumentParser()
parser.add_argument('--install_dir', required=False)
parser.add_argument('file', type=str)
args = parser.parse_args()

with open(args.file, 'w') as f:
    f.write('%s' % (time.time(),))
