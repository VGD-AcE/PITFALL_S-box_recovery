#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <set>
#include <algorithm>
#include <random>
#include <iterator>
#include <numeric>
#include <chrono>
#include <iomanip> 
#include "libxl.h"

using namespace libxl;

// Define the function of Ascon's fault injection process as a structure, and the return value contains an integer and a decimal, so that multiple results can be returned.
// (Numbers of random-word and average random-nibble fault injections required to recover 64 S-box values in a single experiment).
struct Result {
    int returnFaultRound;
    double returnFaultNibble;
};

int S[32] = { 4,11,31,20,26,21,9,2,27,5,8,18,29,3,6,28,
               30,19,7,14,0,13,17,24,16,12,1,25,22,10,15,23 };     //5-bit S-box in Ascon

//int S_1[16] = { 4,11,31,20,27,5,8,18,30,19,7,14,16,12,1,25 };     //stuck-at-0 fault at 3rd bit
int S_1[16] = { 26,21,9,2,29,3,6,28,0,13,17,24,22,10,15,23 };     //stuck-at-1 fault at 3rd bit

int Sbox[64] = { 0 };     // Store the correct input value of the S-box of the final round of Ascon's Finalization.
int f_Sbox[64] = { 0 };     // Store the incorrect input value of the S-box of the final round of Ascon's Finalization (remove the 3rd bit).
int fault[64] = { 0 };     // Store the nibble fault value of the S-box of the final round of Ascon's Finalization.


void set_Sbox(int Sbox[]) {
    // Define a random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distribution(0, 31);
    for (int i = 0; i < 64; ++i) {
        int randomNum = distribution(gen);
        Sbox[i] = randomNum;
    }
}

void set_fault(int F[]) {
    // Define a random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distribution(0, 15);
    for (int i = 0; i < 64; ++i) {
        int randomNum = distribution(gen);
        F[i] = randomNum;
    }
}

void delete_3rd_bit(int S[]) {

    int temp_s[5];
    int result;
    for (int i = 0; i < 64; ++i) {
        temp_s[0] = Sbox[i] % 2;
        temp_s[1] = (Sbox[i] / 2) % 2;
        temp_s[2] = (Sbox[i] / 4) % 2;
        temp_s[3] = (Sbox[i] / 8) % 2;
        temp_s[4] = (Sbox[i] / 16) % 2;

        result = temp_s[4] * 8 + temp_s[3] * 4 + temp_s[1] * 2 + temp_s[0];

        // Store results back into S-box array
        Sbox[i] = result;
    }
}

// Function: Find the intersection of two sets
std::vector<int> calculateIntersection(const std::vector<int>& set1, const std::vector<int>& set2) {
    // If two out-of-order sets execute this function, they must be sorted first.

    std::vector<int> intersection;
    std::set_intersection(
        set1.begin(), set1.end(),
        set2.begin(), set2.end(),
        std::back_inserter(intersection)
    );
    return intersection;
}

Result Ascon_trial(Sheet* sheet, int Num)
{
    int out;
    // Defines a three-dimensional variable-length array: 16*4*?
    std::vector<std::vector<std::vector<int>>> differ_LSB_2(16, std::vector<std::vector<int>>(4));

    // Solving differential equations
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 32; j++) {
            for (int in = 0; in < 16; in++) {
                out = S_1[in] ^ S_1[i ^ in];
                if (j == out) {
                    differ_LSB_2[i][j % 4].push_back(in);
                }
            }
            std::sort(differ_LSB_2[i][j % 4].begin(), differ_LSB_2[i][j % 4].end()); // Reorder the collection
        }
    }

    // Call function to set array
    set_Sbox(Sbox);

    // Print the 5-bit S-box value in row Num and column 2 in the Excel table
    std::wstring S;
    for (int i = 0; i < 64; ++i) {
        S += std::to_wstring(Sbox[i]) + L",";
    }
    sheet->writeStr(Num, 1, S.c_str());

    // Fault injections: stuck-at fault
    delete_3rd_bit(Sbox);

    // Fault injections: Random-nibble fault
    //    
    // The number of fault injections is Count
    const int COUNT = 100;
    int count;
    // When temp is 64, it means that the intersection of possible values of each S-box is unique, and the experiment ends at this time.
    int temp = 0;
    // Define a three-dimensional variable-length array: COUNT*64*? , to store the set of possible values of the S-box after the count fault injection.
    std::vector<std::vector<std::vector<int>>> Intersection(COUNT, std::vector<std::vector<int>>(64));
    // Define a two-dimensional variable-length array: 64*?, to store the intersection of possible values of two rows of S-boxes.
    std::vector<std::vector<int>> Intersec(64);

    std::wstring Count_nibble;
    // Define a Countnibble array to store the number of fault action rounds when the possible input values of all S-boxes are uniquely determined (number of random-word fault injections).
    int Countnibble[64] = { 0 };

    for (count = 0; count < COUNT; ++count) {

        // Call function injecting faults
        set_fault(fault);

        // Print the 4-bit fault value in the "Num" row, 5th, 8th, 11th,...column
        std::wstring f;
        for (int i = 0; i < 64; ++i) {
            f += std::to_wstring(fault[i]) + L",";
        }
        sheet->writeStr(Num, 5 + count * 3, f.c_str());

        // fault injections
        for (int i = 0; i < 64; ++i) {
            f_Sbox[i] = Sbox[i] ^ fault[i];
        }

        // Print the S-box output difference in the "Num" row, 6th, 9th, 12th,...column
        std::wstring dif;
        for (int i = 0; i < 64; ++i) {
            dif += std::to_wstring(S_1[Sbox[i]] ^ S_1[f_Sbox[i]]) + L",";
        }
        sheet->writeStr(Num, 6 + count * 3, dif.c_str());

        // Fill the array
        for (int i = 0; i < 64; ++i) {
            Intersection[count][i] = differ_LSB_2[fault[i]][(S_1[Sbox[i]] ^ S_1[f_Sbox[i]]) % 4];
        }

        // When a fault is injected for the first time, the S-box intersection set is initialized to the possible values of the S-box at this time.
        // Print the set of 64 S-box possible values in "Num" row, 7th column
        std::wstring Sb;
        if (count == 0) {
            for (int i = 0; i < 64; ++i) {
                Intersec[i] = Intersection[0][i];
                Sb += L"{";
                for (int j = 0; j < Intersec[i].size(); ++j) {
                    Sb += std::to_wstring(Intersec[i][j]) + L" ";
                }
                Sb += L"},";
                sheet->writeStr(Num, 7, Sb.c_str());
            }
        }

        // When a fault is subsequently injected, the newly obtained S-box possible values and the existing S-box possible value set are intersected.
        // Print the set of 64 S-box possible values in "Num" row, 10th, 13th, 16th,...column
        std::wstring Sbb;
        if (count > 0) {
            for (int i = 0; i < 64; ++i) {
                Intersec[i] = calculateIntersection(Intersection[count][i], Intersec[i]);
                Sbb += L"{";
                for (int j = 0; j < Intersec[i].size(); ++j) {
                    Sbb += std::to_wstring(Intersec[i][j]) + L" ";
                }
                Sbb += L"},";
                sheet->writeStr(Num, 7 + count * 3, Sbb.c_str());
                temp += Intersec[i].size();
                if ((Intersec[i].size() == 1) && (Countnibble[i] == 0)) {
                    // When the S-box has a unique possible value for the first time, record the number of fault action rounds at this time.
                    Countnibble[i] = count + 1;
                }
            }
            if (temp == 64) {
                break;
            }
            else {
                temp = 0;
            }
        }
    }
    int sum = 0;
    for (int i = 0; i < 64; ++i) {
        Count_nibble += std::to_wstring(Countnibble[i]) + L",";
        sum += Countnibble[i];
    }
    double Average_Nibble = double(sum) / 64;
    Count_nibble += L"\nThe average fault nibbles required for 64 S-boxes is:" + std::to_wstring(Average_Nibble);

    // Output all results in an experiment to excel table
    // 
    // Print the 4bit S* box value in "Num" row, 2nd column
    std::wstring Sf;
    for (int i = 0; i < 64; ++i) {
        Sf += std::to_wstring(Sbox[i]) + L",";
    }
    sheet->writeStr(Num, 2, Sf.c_str());

    // Print the minimum number of experiments required to obtain a unique S-box input value in "Num" row, 3rd column
    std::wstring newNumberStr = std::to_wstring(count + 1);
    sheet->writeStr(Num, 3, newNumberStr.c_str());

    // Print the number of nibble fault injections required for recovery of a single S-box input value in "Num" row, 4th column
    sheet->writeStr(Num, 4, Count_nibble.c_str());

    Result result;
    result.returnFaultRound = count + 1;
    result.returnFaultNibble = Average_Nibble;

    return result;

}

int main()
{
    // Program timing starts
    auto start = std::chrono::high_resolution_clock::now();

    const int trial_Num = 10000;
    int Count[trial_Num] = { 0 };
    double Countnibble[trial_Num] = { 0 };
    int temp = 0;
    double temp1 = 0;

    // Create an Excel document object
    libxl::Book* book = xlCreateBook();
    book->setKey(L"libxl", L"windows-28232b0208c4ee0369ba6e68abv6v5i3");
    if (book) {

        libxl::Sheet* sheet = book->addSheet(L"Sheet1");// Add a worksheet

        //sheet->setCol(1, 0, 30); // Set table column width
        //sheet->setCol(1, 1, 35);
        //sheet->setCol(1, 2, 35);
        //sheet->setCol(1, 3, 35);
        //sheet->setCol(1, 4, 35);

        // Set table row titles
        sheet->writeStr(0, 1, L"5bit S-box");
        sheet->writeStr(0, 2, L"4bit S* box");
        sheet->writeStr(0, 3, L"The minimum number of experiments required to obtain a unique S-box input value");
        sheet->writeStr(0, 4, L"Number of nibble fault injections required to recover a single S-box input value");
        for (int i = 0; i < 100; ++i) {// Each set of experimental settings can have up to 100 fault injections
            // Convert i to wide string
            std::wstring i_str = std::to_wstring(i + 1);

            // Construct the string to be output
            std::wstring output_str_1 = L"4-bit fault values in" + i_str + L"th fault injection";
            std::wstring output_str_2 = L"4-bit output difference of all S* boxes in" + i_str + L"th fault injection";
            std::wstring output_str_3 = L"The set of possible input values of all S* boxes after" + i_str + L"th fault injection";

            sheet->writeStr(0, 5 + 3 * i, output_str_1.c_str());
            sheet->writeStr(0, 6 + 3 * i, output_str_2.c_str());
            sheet->writeStr(0, 7 + 3 * i, output_str_3.c_str());
        }

        // Set table column titles
        for (int i = 0; i < trial_Num; ++i) {
            std::wstring i_str = std::to_wstring(i + 1);
            std::wstring output_str = L"Trial serial number" + i_str + L":";
            sheet->writeStr(i + 1, 0, output_str.c_str());

            Result result = Ascon_trial(sheet, i + 1);
            Count[i] = result.returnFaultRound;
            Countnibble[i] = result.returnFaultNibble;
            temp += Count[i];
            temp1 += Countnibble[i];
        }

        // Timeout
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Calculate the average number of fault injections required to restore all S-box input values in trial_Num experiments, and put the results at the head of the table
        double Average = double(temp) / double(trial_Num);
        double Averagenibble = double(temp1) / double(trial_Num);
        std::wstring newStr = L"The average number of random-word fault is:" + std::to_wstring(Average) + L";The average number of random-nibble fault is:" + std::to_wstring(Averagenibble) + L";The total experimental time is:" + std::to_wstring(duration.count() / 1'000'000.0) + L"s.";
        std::cout << Average << std::endl;
        std::cout << Averagenibble << std::endl;
        sheet->writeStr(0, 0, newStr.c_str());

        // Save Excel file
        book->save(L"Ascon_SDFA_trials.xlsx");

        // Release resources
        book->release();
        std::cout << "Excel file generated successfully!" << std::endl;
    }
    else {
        std::cerr << "Unable to create Excel document object!" << std::endl;
    }

    system("pause");
    return 0;
}

