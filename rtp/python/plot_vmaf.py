import matplotlib.pyplot as plt
import numpy as np
import sys, getopt
import json
import csv
from savitzky_golay_filter import savitzky_golay
logfile_prefix="/home/kyle/test_outputs"

# function to add value labels
def addlabels(x,y):
    for i in range(len(x)):
        plt.text(i, y[i], y[i], ha = 'center')
#argv
"""
0 = name of program
1 = test_name
"""
def main(argv):
    test_folder_name=argv[1]
    file_name=argv[2] + "_vmaf"

    full_filepath = logfile_prefix + "/" + test_folder_name + "/" + file_name + ".json"
    output_file_name=logfile_prefix + "/" + test_folder_name + "/" + "vmaf" + ".png"
    output_file_name_2=logfile_prefix + "/" + test_folder_name + "/" + "vmaf_stats.png"
    output_csv_file_name=logfile_prefix + "/" + test_folder_name + "/" + file_name + ".csv"




    #load
    f = open(full_filepath)
    data = json.load(f)

    # parse
    pooled_metrics = data["pooled_metrics"]
    avg_vmaf_scores = pooled_metrics["vmaf_4k"]

    # iterating to get vmaf of each frame to plot
    vmaf_scores = []
    for frame in data["frames"]:
        vmaf_scores.append(frame["metrics"]["vmaf_4k"])
    
    #package nicely
    vmaf_scores = np.array(vmaf_scores).astype(float)
    vmaf_averages = [avg_vmaf_scores["min"], avg_vmaf_scores["max"], avg_vmaf_scores["mean"], avg_vmaf_scores["harmonic_mean"]]
    
    #plot
    plt.plot(vmaf_scores, "g", linewidth=0.7)
    plt.title("VMAF Index")
    plt.ylabel("VMAF")
    plt.xlabel("Frame Number / 10")
    plt.savefig(output_file_name, dpi=300)

    plt.figure()
    plt.bar(["min", "max", "mean", "harmonic mean"], vmaf_averages, align="center",color=['black', 'green', 'red', 'blue'])
    plt.title("VMAF Averages")
    plt.ylabel("VMAF")
    plt.xlabel("Index")
    addlabels(["min", "max", "mean", "harmonic mean"], vmaf_averages)
    plt.savefig(output_file_name_2, dpi=300)


    # save averages to csv file
    data_file = open(output_csv_file_name, 'w')
    csv_writer = csv.writer(data_file)

    header = ["min", "max", "mean", "harmonic_mean"]
    csv_writer.writerow(header)
    csv_writer.writerow(vmaf_averages)
    data_file.close()

    print("Saved VMAF plot")




if __name__ == "__main__":
    main(sys.argv)
    