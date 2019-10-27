/* This program takes in 2 fractions and an operator and computes the desired problem.
 * It then simplifies and prints the result.
 * Cara Ditmar, Autumn 2019
 */

#include <iostream>

using namespace std;

class fraction {
private:
    // Internal representation of a fraction as two integers
    int numerator;
    int denominator;
public:
    // Class constructor
    fraction(int n, int d);
    // Methods to update the fraction
    void add(fraction f);
    void mult(fraction f);
    void div(fraction f);
    // Simplify fraction
    void simp(void);
    // Display method
    void display(void);
};

int main() {
    int n1 = 0;
    int d1 = 0;
    int n2 = 0;
    int d2 = 0;
    string op;
    char junk;

    // loop until end of file
    while (!cin.fail()) {
        // declare fraction 1
        cin >> n1;
        cin >> junk;    // fraction bar is junk character
        cin >> d1;

        if (!cin.fail()) {
            fraction f1(n1, d1);

            // define operator
            cin >> op;

            // declare fraction 2
            cin >> n2;
            cin >> junk;        // fraction bar is junk character
            cin >> d2;
            fraction f2(n2, d2);

            // apply desired operation to first fraction
            if (op == "+") {
                f1.add(f2);
            } else if (op == "*") {
                f1.mult(f2);
            } else if (op == "div") {
                f1.div(f2);
            }
            // simplify and display fraction
            f1.simp();
            f1.display();
        }
    }
    return 0;
}


/* Constructs a fraction */
fraction::fraction(int n, int d) {
    this->numerator=n;
    this->denominator=d;
}


/* Displays a fraction */
void fraction::display() {
    cout << numerator << " / " << denominator << endl;
}


/* Adds 2 fractions */
void fraction::add(fraction f) {
    numerator = (numerator * f.denominator) + (f.numerator * denominator);
    denominator *= f.denominator;
}


/* Multiplies 2 fractions */
void fraction::mult(fraction f) {
    numerator *= f.numerator;
    denominator *= f.denominator;
}


/* Divides 2 fractions */
void fraction::div(fraction f) {
    numerator *= f.denominator;
    denominator *= f.numerator;
}


/* Simplifies a fraction */
void fraction::simp(void) {
    int i = 2;
    int end;
    // find smaller of the 2 numbers
    if (numerator > denominator) {
        end = denominator;
    } else {
        end = numerator;
    }
    // iterate and repeatedly divide by the least common denominator
    while (i <= end) {
        if (numerator%i==0 && denominator%i==0) {
            numerator /= i;
            denominator /= i;
            i = 2;
        } else {
            i++;
        }
    }
}
