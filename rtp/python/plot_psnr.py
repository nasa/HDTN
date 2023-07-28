import matplotlib.pyplot as plt
import numpy as np
import sys, getopt
from savitzky_golay_filter import savitzky_golay

logfile_prefix="/home/kyle/test_outputs"
#argv
"""
0 = name of program
1 = test_name
"""
def main(argv):
    file_name="psnr_logfile"

    test_folder_name=argv[1]

    output_file_name=logfile_prefix + "/" + test_folder_name + "/" + file_name + ".png"
    full_filepath = logfile_prefix + "/" +test_folder_name + "/" + file_name + ".txt"

    # load data
    matrix = np.loadtxt(full_filepath, dtype=np.string_, delimiter=" ")

    psnr = matrix[:, 5]

    # convert to numbers
    for i in range(len(psnr)):
       psnr[i] = psnr[i][9:]

    # ignore inf
    psnr = psnr.astype(float)
    psnr[psnr== np.inf] = 60

    #plot
    # plt.plot(savitzky_golay(psnr, 11, 3), "r", linewidth=0.7)
    plt.plot(psnr, "r", linewidth=0.7)

    plt.title("Peak Signal to Noise Ratio")
    plt.ylabel("PSNR (dB)")
    plt.xlabel("Frame Number")
    plt.savefig(output_file_name, dpi=300)

    # plt.show()

    print("Saved PSNR plot")


if __name__ == "__main__":
    main(sys.argv)
    