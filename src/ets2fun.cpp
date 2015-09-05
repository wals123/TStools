#include <RcppArmadillo.h>
#include <iostream>
#include <cmath>
// [[Rcpp::depends(RcppArmadillo)]]

using namespace Rcpp;
 
arma::mat matrixpower(arma::mat A, int power){
    arma::mat B = A;
    if(power>1){
        for(int i = 1; i < power; i=i+1){
            B = B * A;
        }
    }
    else if(power==0){
        B.eye();
    }
    return B;
}

double errorf(double yact, double yfit, char Etype){
    if(Etype=='A'){
        return yact - yfit;
    }
    else{
        if(yfit==0){
            return R_PosInf;
        }
        else{
            return (yact - yfit) / yfit;
        }
    }
}

/* # Function is needed to estimate the correct error for ETS when trace model selection with r(matxt) is sorted out. */
arma::mat errorvf(arma::mat yact, arma::mat yfit, char Etype){
    if(Etype=='A'){
        return yact - yfit;
    }
    else{
        yfit.elem(find(yfit==0)).fill(1e-100);
        return (yact - yfit) / yfit;
    }
}

arma::rowvec rvalue(arma::rowvec matxt, arma::rowvec matrixw, char Etype, char Ttype, char Stype, int ncomponents){
    arma::rowvec r(ncomponents);
    arma::rowvec xtnew;

    xtnew = matxt % matrixw;

    if(Etype=='A'){
        if(Ttype=='N'){
            if(Stype!='M'){
                r.ones();
            }
            else{
                r(0) = 1 / sum(xtnew(arma::span(1, ncomponents-1)));
                r(arma::span(1, ncomponents-1)).fill(1 / xtnew(0));
            }
        }
        else if(Ttype=='A'){
            if(Stype!='M'){
                r.ones();
            }
            else{
                r(0) = 1 / sum(xtnew(arma::span(2, ncomponents-1)));
                r(1) = 1 / sum(xtnew(arma::span(2, ncomponents-1)));
                r(arma::span(2, ncomponents-1)).fill(1 / sum(xtnew(arma::span(0,1))));
            }
        }
        else if(Ttype=='M'){
            if(Stype=='N'){
                r(0) = 1;
                r(1) = 1 / xtnew(0);
            }
            else if(Stype=='A'){
                r.ones();
                r(1) = 1 / xtnew(0);
            }
            else if(Stype=='M'){
                r(0) = 1 / sum(xtnew(arma::span(2, ncomponents-1)));
                r(1) = 1 / (xtnew(0) * sum(xtnew(arma::span(2, ncomponents-1))));
                r(arma::span(2, ncomponents-1)).fill(1 / (matxt(0) * pow(matxt(1),matrixw(2))));
            }
        }
    }
    else{
        if(Ttype=='N'){
            if(Stype=='N'){
                r(0) = xtnew(0);
            }
            else if(Stype=='A'){
                r.fill(sum(matxt * trans(matrixw)));
            }
            else if(Stype=='M'){
                r(0) = matxt(0);
                r(arma::span(1, ncomponents-1)) = matxt(arma::span(1, ncomponents-1));
            }
        }
        else if(Ttype=='A'){
            if(Stype!='M'){
                r.fill(sum(matxt * trans(matrixw)));
            }
            else if(Stype=='M'){
                r(0) = sum(matxt(arma::span(0,1)) * trans(matrixw(arma::span(0,1))));
                r(1) = r(0);
                r(arma::span(2, ncomponents-1)) = matxt(arma::span(2, ncomponents-1));
            }
        }
        else if(Ttype=='M'){
            if(Stype=='N'){
                r(0) = exp(sum(log(matxt(arma::span(0,1))) * trans(matrixw(arma::span(0,1)))));
                r(1) = exp(matrixw(1) * log(matxt(1)));
            }
            else if(Stype=='A'){
                r(0) = exp(sum(log(matxt(arma::span(0,1))) * trans(matrixw(arma::span(0,1))))) + sum(xtnew(arma::span(2, ncomponents-1)));
                r(1) = r(0) / matxt(0);
                r(arma::span(2, ncomponents-1)).fill(r(0));
            }
            else if(Stype=='M'){
                r(0) = exp(sum(log(matxt(arma::span(0,1))) * trans(matrixw(arma::span(0,1)))));
                r(1) = exp(matrixw(1) * log(matxt(1)));
                r(arma::span(2, ncomponents-1)).fill(sum(xtnew(arma::span(2, ncomponents-1))));
            }
        }
    }

    return r;
}

arma::mat avalue(int freq, double(error), double gamma, double yfit, char E, char S, char T){
    arma::mat a(1,freq);
    if(S=='A'){
        if(E=='M'){
            a.fill(gamma / freq * yfit * error);
        }
        else{
            a.fill(gamma / freq * error);
        }
    }
    else if(S=='M'){
        if(E=='M'){
            a.fill(1 + gamma / freq * yfit * error);
        }
        else{
            a.fill(1 + gamma / freq * error);
        }
    }
    else{
        a.fill(0);
    }
    return(a);
}

List fitter(arma::mat matrixxt,arma::mat  matrixF,arma::mat  matrixw,arma::mat  matyt,arma::mat  matg,char E,char T,char S,int freq) {
    int obs = matyt.n_rows;
    int freqtail = 0;
    int j;
    int ncomponents;
    int ncomponentsall;

    arma::mat matyfit(obs, 1, arma::fill::zeros);
    arma::mat materrors(obs, 1, arma::fill::zeros);

    arma::mat matrixxtnew;
    arma::mat matrixwnew;
    arma::mat matrixFnew;
    arma::mat matgnew;
    arma::mat dummy(freq,freq, arma::fill::eye);
    arma::mat avec(1,freq);

/* # matxt is transformed into obs by n.components+freq matrix, where last freq are seasonal coefficients,
# matw is matrix obs by n.components+freq with dummies in freq parts,
# matF is a matrix as in Hyndman et al.
# vecg is the vector of 
# dummy contains dummy variables for seasonal coefficients
*/
    if(S!='N'){
        ncomponents = matrixxt.n_cols-1;
        ncomponentsall = ncomponents + freq;

        matgnew.set_size(obs,ncomponentsall);
        matgnew.cols(0,ncomponents-1).each_row() = trans(matg.rows(0,ncomponents-1));

/* # Fill in matxt with provided initial level and trend and then - with the provided initial seasonals */
        matrixxtnew.set_size(obs+1,ncomponentsall);
        matrixxtnew.zeros();
        matrixxtnew.submat(0, 0, 0, ncomponents-1) = matrixxt.submat(0, 0, 0, ncomponents-1);
        matrixxtnew.submat(0, ncomponents, 0, ncomponentsall-1) = trans(matrixxt.submat(0, ncomponents, freq-1, ncomponents));

        matrixwnew.set_size(obs,ncomponentsall);
        matrixFnew.eye(ncomponentsall,ncomponentsall);
/* # Fill in the matrix matw and cube matF with the provided values of matw and matF */
        for(int i=0; i<ncomponents; i=i+1){
            matrixwnew.col(i).each_row() = matrixw.col(i);
            matrixFnew(i,0) = matrixF(i,0);
            if(ncomponents>1){
                matrixFnew(i,1) = matrixF(i,1);
            }
        }

/* # Fill in dummies for the seasonal components */
        for(int i=0; i < std::floor(obs/freq); i=i+1){
            matrixwnew.submat(i*freq, ncomponents, (i+1)*freq-1, ncomponentsall-1) = dummy;
            matgnew.submat(i*freq, ncomponents, (i+1)*freq-1, ncomponentsall-1) = dummy * matg(ncomponents,0);
        }

        freqtail = obs - freq * std::floor(obs/freq);
        if(freqtail!=0){
            j = 0;
            for(int i=obs-freqtail; i<obs; i=i+1){
                matrixwnew.submat(i, ncomponents, i, ncomponents-1+freq) = dummy.row(j);
                matgnew.submat(i, ncomponents, i, ncomponents-1+freq) = dummy.row(j) * matg(ncomponents,0);
                j=j+1;
            }
        }
    }
    else{
        ncomponents = matrixxt.n_cols;
        ncomponentsall = ncomponents;
        matrixxtnew = matrixxt;
        matgnew.set_size(obs,ncomponents);
        matgnew.each_row() = trans(matg);

        matrixwnew.set_size(obs,ncomponents);
        matrixFnew.eye(ncomponents,ncomponents);

/* # Fill in the matrices matw and matF with the provided values of matw and matF. Transform matF into the one used in Hyndman et al. */
        for(int i=0; i<ncomponents; i=i+1){
            matrixwnew.col(i).each_row() = matrixw.col(i);
            matrixFnew(i,0) = matrixF(i,0);
            if(ncomponents>1){
                matrixFnew(i,1) = matrixF(i,1);
            }
        }
    }

    if((T!='M') & (S!='M')){
        for (int i=0; i<obs; i=i+1) {
            matyfit.row(i) = matrixwnew.row(i) * trans(matrixxtnew.row(i));
            materrors(i,0) = errorf(matyt(i,0), matyfit(i,0), E);
            matrixxtnew.row(i+1) = matrixxtnew.row(i) * trans(matrixFnew) + trans(materrors.row(i)) * matgnew.row(i) % rvalue(matrixxtnew.row(i), matrixwnew.row(i), E, T, S, ncomponentsall);
/*            if(S=='A'){
                avec.fill(as_scalar(mean(matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1),1)));
                matrixxtnew(i+1,0) = matrixxtnew(i+1,0) + avec(0,0);
                matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1) = matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1) - avec;
            } */
        }
    }
    else if((T!='A') & (S!='A')){
        if((T=='M') | (S=='M')){
            for (int i=0; i<obs; i=i+1) {
                matyfit.row(i) = exp(matrixwnew.row(i) * log(trans(matrixxtnew.row(i))));
                materrors(i,0) = errorf(matyt(i,0), matyfit(i,0), E);
                matrixxtnew.row(i+1) = exp(log(matrixxtnew.row(i)) * trans(matrixFnew)) + trans(materrors.row(i)) * matgnew.row(i) % rvalue(matrixxtnew.row(i), matrixwnew.row(i), E, T, S, ncomponentsall);
                if(arma::as_scalar(matrixxtnew(i+1,0))<0){
                    matrixxtnew(i+1,0) = 0.0001;
                }
                if(arma::as_scalar(matrixxtnew(i+1,1))<0){
                    matrixxtnew(i+1,1) = 0.0001;
                }
/*                if(S=='M'){
                    avec.fill(as_scalar(exp(mean(log(matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1)),1))));
                    matrixxtnew(i+1,0) = matrixxtnew(i+1,0) * avec(0,0);
                    matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1) = matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1) / avec;
                }
                a = avalue(freq, double(materrors(i,0)), double(matgnew(i,ncomponents)), double(matyfit(i,0)), E, S, T);
                matrixxtnew(i+1,0) = matrixxtnew(i+1,0) + a(0,0);
                matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall) = matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall) - a; */
            }
        }
    }
    else if((T=='A') & (S=='M')){
        for (int i=0; i<obs; i=i+1) {
            matyfit.row(i) = matrixwnew.row(i).cols(0,1) * trans(matrixxtnew.row(i).cols(0,1)) * sum(matrixwnew.row(i).cols(2,ncomponentsall-1) % matrixxtnew.row(i).cols(2,ncomponentsall-1));
            materrors(i,0) = errorf(matyt(i,0), matyfit(i,0), E);
            matrixxtnew.row(i+1) = matrixxtnew.row(i) * trans(matrixFnew) + trans(materrors.row(i)) * matgnew.row(i) % rvalue(matrixxtnew.row(i), matrixwnew.row(i), E, T, S, ncomponentsall);
            matrixxtnew.elem(find(matrixxtnew.row(i+1).cols(2,ncomponentsall-1)<0)).zeros();
/*            avec.fill(as_scalar(exp(mean(log(matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1)),1))));
            matrixxtnew(i+1,0) = matrixxtnew(i+1,0) * avec(0,0);
            matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1) = matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1) / avec; */
        }
    }
    else if((T=='M') & (S=='A')){
        for (int i=0; i<obs; i=i+1) {
            if(arma::as_scalar(matrixxtnew(i,1))<0){
                matrixxtnew(i+1,0) = matrixxtnew(i,0) * matrixxtnew(i,1);
                matrixxtnew(i+1,1) = matrixxtnew(i,1);
                matyfit.row(i) = matrixxtnew(i,0) * matrixxtnew(i,1) + sum(matrixwnew.row(i).cols(2,ncomponentsall-1) % matrixxtnew.row(i).cols(2,ncomponentsall-1));
            }
            else{
                matrixxtnew(i+1,0) = matrixxtnew(i,0) * pow(matrixxtnew(i,1),matrixFnew(0,1));
                matrixxtnew(i+1,1) = pow(matrixxtnew(i,1),matrixFnew(1,1));
                matyfit.row(i) = exp(matrixwnew.row(i).cols(0,1) * log(trans(matrixxtnew.row(i).cols(0,1)))) + sum(matrixwnew.row(i).cols(2,ncomponentsall-1) % matrixxtnew.row(i).cols(2,ncomponentsall-1));
            }
            if(arma::as_scalar(matyfit.row(i))<0){
                matyfit.row(i) = 0;
            }
            materrors(i,0) = errorf(matyt(i,0), matyfit(i,0), E);
            matrixxtnew.row(i+1).cols(0,1) = matrixxtnew.row(i+1).cols(0,1) + trans(materrors.row(i)) * matgnew.row(i).cols(0,1) % rvalue(matrixxtnew.row(i), matrixwnew.row(i), E, T, S, ncomponentsall).cols(0,1);
            matrixxtnew.row(i+1).cols(2,ncomponentsall-1) = matrixxtnew.row(i).cols(2,ncomponentsall-1) * trans(matrixFnew.submat(2,2,ncomponentsall-1,ncomponentsall-1)) + trans(materrors.row(i)) * matgnew.row(i).cols(2,ncomponentsall-1) % rvalue(matrixxtnew.row(i), matrixwnew.row(i), E, T, S, ncomponentsall).cols(2,ncomponentsall-1);
            if((arma::as_scalar(matrixxtnew(i+1,0))<0) || (std::isnan(arma::as_scalar(matrixxtnew(i+1,0))))){
                matrixxtnew(i+1,0) = 0.0001;
            }
            if((arma::as_scalar(matrixxtnew(i+1,1))<0) || (std::isnan(arma::as_scalar(matrixxtnew(i+1,1))))){
                matrixxtnew(i+1,1) = 0.0001;
            }
/*            avec.fill(as_scalar(mean(matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1),1)));
            matrixxtnew(i+1,0) = (matrixxtnew(i+1,0) + avec(0,0));
            matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1) = matrixxtnew.submat(i+1, ncomponents, i+1, ncomponentsall-1) - avec; */
        }
    }

// # Fill in matxt for the seasonal models
    if(S!='N'){
        matrixxt.submat(freq-1,0,matrixxt.n_rows-1,ncomponents-1) = matrixxtnew.cols(0,ncomponents-1);

        for(int i=0; i < std::floor(obs/freq); i=i+1){
            matrixxt.submat((i+1)*freq,ncomponents,(i+2)*freq-1,ncomponents) = matrixxtnew.submat(i*freq+1,ncomponents,(i+1)*freq,ncomponentsall-1).diag();
        }

        if(freqtail!=0){
            j = 0;
            for(unsigned int i=matrixxtnew.n_rows-freqtail; i<matrixxtnew.n_rows; i=i+1){
                matrixxt(i+freq-1, ncomponents) = matrixxtnew(i, ncomponents+j);
                j=j+1;
            }
        }
    }
    else{
        matrixxt = matrixxtnew;
    }

    return List::create(Named("matxt") = matrixxt, Named("yfit") = matyfit, Named("errors") = materrors);
}


// [[Rcpp::export]]
RcppExport SEXP fitterwrap(SEXP matxt, SEXP matF, SEXP matw, SEXP yt, SEXP vecg, SEXP Etype, SEXP Ttype, SEXP Stype, SEXP seasfreq) {
    NumericMatrix mxt(matxt);
    arma::mat matrixxt(mxt.begin(), mxt.nrow(), mxt.ncol(), false);
    NumericMatrix mF(matF);
    arma::mat matrixF(mF.begin(), mF.nrow(), mF.ncol(), false);
    NumericMatrix vw(matw);
    arma::mat matrixw(vw.begin(), vw.nrow(), vw.ncol(), false);
    NumericMatrix vyt(yt);
    arma::mat matyt(vyt.begin(), vyt.nrow(), vyt.ncol(), false);
    NumericMatrix vg(vecg);
    arma::mat matg(vg.begin(), vg.nrow(), vg.ncol(), false);
    char E = as<char>(Etype);
    char T = as<char>(Ttype);
    char S = as<char>(Stype);
    int freq = as<int>(seasfreq);
    
    return fitter(matrixxt, matrixF, matrixw, matyt, matg, E, T, S, freq);
}

arma::mat forecaster(arma::mat matrixxt, arma::mat matrixF, arma::mat matrixw, int hor, char T, char S, int freq) {
    int hh;

    arma::mat matyfor(hor, 1, arma::fill::zeros);
    arma::mat matrixxtnew;
    arma::mat matrixwnew;
    arma::mat matrixFnew;
    arma::mat seasxt(hor, 1, arma::fill::zeros);

    if(S!='N'){
        hh = std::min(hor,freq);
        seasxt.submat(0, 0, hh-1, 0) = matrixxt.submat(0,matrixxt.n_cols-1,hh-1,matrixxt.n_cols-1);

        if(hor > freq){
            for(int i = freq; i < hor; i=i+1){
                seasxt.row(i) = seasxt.row(i-freq);
            }
        }

        matrixxtnew = matrixxt.submat(matrixxt.n_rows-1, 0, matrixxt.n_rows-1, matrixxt.n_cols-2);
        matrixwnew = matrixw.cols(0, matrixw.n_cols-2);
        matrixFnew = matrixF.submat(0, 0, matrixF.n_rows-2, matrixF.n_cols-2);
    }
    else{
        matrixxtnew = matrixxt.submat(matrixxt.n_rows-1, 0, matrixxt.n_rows-1, matrixxt.n_cols-1);
        matrixwnew = matrixw;
        matrixFnew = matrixF;
    }

    if(T!='M'){
        for(int i = 0; i < hor; i=i+1){
            matyfor.row(i) = matrixwnew * matrixpower(matrixFnew,i) * trans(matrixxtnew);
        }
    }
    else{
        for(int i = 0; i < hor; i=i+1){
            matyfor.row(i) = exp(matrixwnew * matrixpower(matrixFnew,i) * log(trans(matrixxtnew)));
        }
    }
    if(S=='A'){
        matyfor = matyfor + seasxt;
    }
    else if(S=='M'){
        matyfor = matyfor % seasxt;
    }

    return matyfor;
}

// [[Rcpp::export]]
RcppExport arma::mat forecasterwrap(SEXP matxt, SEXP matF, SEXP matw, SEXP h, SEXP Ttype, SEXP Stype, SEXP seasfreq){
    NumericMatrix mxt(matxt);
    arma::mat matrixxt(mxt.begin(), mxt.nrow(), mxt.ncol(), false);
    NumericMatrix mF(matF);
    arma::mat matrixF(mF.begin(), mF.nrow(), mF.ncol(), false);
    NumericMatrix vw(matw);
    arma::mat matrixw(vw.begin(), vw.nrow(), vw.ncol(), false);
    int hor = as<int>(h);
    char T = as<char>(Ttype);
    char S = as<char>(Stype);
    int freq = as<int>(seasfreq);
    
    return(forecaster(matrixxt, matrixF, matrixw, hor, T, S, freq));
}

arma::mat errorer(arma::mat matrixxt, arma::mat matrixF, arma::mat matrixw, arma::mat matyt, int hor, char E, char T, char S, int freq, bool tr) {
    int obs = matyt.n_rows;
    int hh;
    arma::mat materrors;

    if(tr==true){
        materrors.set_size(obs, hor);
        materrors.fill(NA_REAL);
    }
    else{
        materrors.set_size(obs, 1);
    }

    if(tr==true){
        for(int i = 0; i < obs; i=i+1){
            hh = std::min(hor, obs-i);
            materrors.submat(i, 0, i, hh-1) = trans(errorvf(matyt.rows(i, i+hh-1), forecaster(matrixxt.rows(i,i+freq-1), matrixF, matrixw, hh, T, S, freq), E));
        }
    }
    else{
      for(int i = 0; i < obs; i=i+1){
	    materrors.row(i) = trans(errorvf(matyt.row(i), forecaster(matrixxt.rows(i,i+freq-1), matrixF, matrixw, 1, T, S, freq), E));
	    }
    }
//std::cout << materrors;
    return materrors;
}

// [[Rcpp::export]]
RcppExport arma::mat errorerwrap(SEXP matxt, SEXP matF, SEXP matw, SEXP yt, SEXP h, SEXP Etype, SEXP Ttype, SEXP Stype, SEXP seasfreq, SEXP trace){
    NumericMatrix mxt(matxt);
    arma::mat matrixxt(mxt.begin(), mxt.nrow(), mxt.ncol(), false);
    NumericMatrix mF(matF);
    arma::mat matrixF(mF.begin(), mF.nrow(), mF.ncol(), false);
    NumericMatrix vw(matw);
    arma::mat matrixw(vw.begin(), vw.nrow(), vw.ncol(), false);
    NumericMatrix vyt(yt);
    arma::mat matyt(vyt.begin(), vyt.nrow(), vyt.ncol(), false);
    int hor = as<int>(h);
    char E = as<char>(Etype);
    char T = as<char>(Ttype);
    char S = as<char>(Stype);
    int freq = as<int>(seasfreq);
    bool tr = as<bool>(trace);

    return errorer(matrixxt, matrixF, matrixw, matyt, hor, E, T, S, freq, tr);
}


double optimizer(arma::mat matrixxt,arma::mat matrixF,arma::mat matrixw,arma::mat matyt,arma::mat matg,int hor,char E,char T,char S,int freq,bool tr,std::string CFtype,int normalize){
    int obs = matyt.n_rows;
    double CFres = 0;
    int matobs = obs - hor + 1;
    double yactsum = arma::as_scalar(arma::sum(log(matyt)));

    List fitting = fitter(matrixxt,matrixF,matrixw,matyt,matg,E,T,S,freq);
    NumericMatrix mxtfromfit = as<NumericMatrix>(fitting["matxt"]);
    matrixxt = as<arma::mat>(mxtfromfit);
    NumericMatrix errorsfromfit = as<NumericMatrix>(fitting["errors"]);

    arma::mat materrors;

    if(E=='M'){
        if(tr==true){
            materrors = log(1 + errorer(matrixxt, matrixF, matrixw, matyt, hor, E, T, S, freq, tr));
            materrors.elem(find_nonfinite(materrors)).fill(1e10);
            if(CFtype=="GV"){
                materrors.resize(matobs,hor);
                CFres = double(log(arma::det(arma::trans(materrors) * materrors / double(matobs))));
                CFres = CFres + (2 / double(matobs)) * double(hor) * yactsum;
            }
            else if(CFtype=="TLV"){
                for(int i=0; i<hor; i=i+1){
                    CFres = CFres + arma::as_scalar(log(mean(pow(materrors.submat(0,i,obs-i-1,i),2))));
                }
                CFres = CFres + (2 / double(obs)) * double(hor) * yactsum;
            }
            else if(CFtype=="TV"){
                for(int i=0; i<hor; i=i+1){
                    CFres = CFres + arma::as_scalar(mean(pow(materrors.submat(0,i,obs-i-1,i),2)));
                }
                CFres = exp(log(CFres) + (2 / double(obs)) * double(hor) * yactsum);
            }
            else if(CFtype=="hsteps"){
                CFres = arma::as_scalar(exp(log(mean(pow(materrors.submat(0,hor-1,obs-hor,hor-1),2))) + (2 / double(obs)) * yactsum));
            }
        }
        else{
            arma::mat materrors(errorsfromfit.begin(), errorsfromfit.nrow(), errorsfromfit.ncol(), false);
            materrors = log(1+materrors);
            CFres = arma::as_scalar(exp(log(mean(pow(materrors,2))) + (2 / double(obs)) * yactsum));
        }
    }
    else{
        if(tr==true){
            materrors = errorer(matrixxt, matrixF, matrixw, matyt, hor, E, T, S, freq, tr);
            if(CFtype=="GV"){
                materrors.resize(matobs,hor);
                materrors = materrors / normalize ;
                CFres = double(log(det(trans(materrors) * (materrors) / matobs)) + hor * log(matobs * pow(normalize,2)));
            }
            else if(CFtype=="TLV"){
                for(int i=0; i<hor; i=i+1){
                    CFres = CFres + arma::as_scalar(log(mean(pow(materrors.submat(0,i,obs-i-1,i),2))));
                }
            }
            else if(CFtype=="TV"){
                for(int i=0; i<hor; i=i+1){
                    CFres = CFres + arma::as_scalar(mean(pow(materrors.submat(0,i,obs-i-1,i),2)));
                }
            }
            else if(CFtype=="hsteps"){
                CFres = arma::as_scalar(mean(pow(materrors.submat(0,hor-1,obs-hor,hor-1),2)));
            }
        }
        else{
            arma::mat materrors(errorsfromfit.begin(), errorsfromfit.nrow(), errorsfromfit.ncol(), false);
            CFres = arma::as_scalar(mean(pow(materrors,2)));
        }
    }
    return CFres;
}

// [[Rcpp::export]]
RcppExport double optimizerwrap(SEXP matxt, SEXP matF, SEXP matw, SEXP yt, SEXP vecg, SEXP h, SEXP Etype, SEXP Ttype, SEXP Stype, SEXP seasfreq, SEXP trace, SEXP CFt, SEXP normalizer) {
    NumericMatrix mxt(matxt);
    arma::mat matrixxt(mxt.begin(), mxt.nrow(), mxt.ncol());
    NumericMatrix mF(matF);
    arma::mat matrixF(mF.begin(), mF.nrow(), mF.ncol(), false);
    NumericMatrix vw(matw);
    arma::mat matrixw(vw.begin(), vw.nrow(), vw.ncol(), false);
    NumericMatrix vyt(yt);
    arma::mat matyt(vyt.begin(), vyt.nrow(), vyt.ncol(), false);
    NumericMatrix vg(vecg);
    arma::mat matg(vg.begin(), vg.nrow(), vg.ncol(), false);
    int hor = as<int>(h);
    char E = as<char>(Etype);
    char T = as<char>(Ttype);
    char S = as<char>(Stype);
    int freq = as<int>(seasfreq);
    bool tr = as<bool>(trace);
    std::string CFtype = as<std::string>(CFt);
    double normalize = as<double>(normalizer);
    
    return optimizer(matrixxt,matrixF,matrixw,matyt,matg,hor,E,T,S,freq,tr,CFtype,normalize);
}


double Thetafunc(double Theta, arma::vec matg, double phivalue, int freq){
    return(abs(phivalue*matg(0)+phivalue+1)/(matg(2)) + ((phivalue-1)*(1+cos(Theta)-cos(freq*Theta))+cos((freq-1)*Theta)-phivalue*cos((freq+1)*Theta))/(2*(1+cos(Theta))*(1-cos(freq*Theta))));
}

// [[Rcpp::export]]
RcppExport double costfunc(SEXP matxt, SEXP matF, SEXP matw, SEXP yt, SEXP vecg, SEXP h, SEXP Etype, SEXP Ttype, SEXP Stype, SEXP seasfreq, SEXP trace, SEXP CFt, SEXP normalizer, SEXP bounds, SEXP phi, SEXP Theta) {
/* Function is needed to implement constrains on smoothing parameters */
    NumericMatrix mxt(matxt);
    arma::mat matrixxt(mxt.begin(), mxt.nrow(), mxt.ncol());
    NumericMatrix mF(matF);
    arma::mat matrixF(mF.begin(), mF.nrow(), mF.ncol(), false);
    NumericMatrix vw(matw);
    arma::mat matrixw(vw.begin(), vw.nrow(), vw.ncol(), false);
    NumericMatrix vyt(yt);
    arma::vec matyt(vyt.begin(), vyt.nrow(), vyt.ncol(), false);
    NumericMatrix vg(vecg);
    arma::vec matg(vg.begin(), vg.nrow(), vg.ncol());
    int hor = as<int>(h);
    char E = as<char>(Etype);
    char T = as<char>(Ttype);
    char S = as<char>(Stype);
    int freq = as<int>(seasfreq);
    bool tr = as<bool>(trace);
    std::string CFtype = as<std::string>(CFt);
    double normalize = as<double>(normalizer);

    char boundtype = as<char>(bounds);
    double phivalue = as<double>(phi);
    double theta = as<double>(Theta);

    if(boundtype='u'){
// alpha in (0,1)
        if((matg(0)>1) || (matg(0)<0)){
            matg.zeros();
            matrixxt.zeros();
        }
        if(T!='N'){
// beta in (0,alpha)
            if((matg(1)>matg(0)) || (matg(1)<0)){
                matg.zeros();
                matrixxt.zeros();
            }
            if(S!='N'){
// gamma in (0,1-alpha)
                if((matg(2)>(1-matg(0))) || (matg(2)<0)){
                    matg.zeros();
                    matrixxt.zeros();
                }
            }
        }
        if(S!='N'){
// gamma in (0,1-alpha)
            if((matg(1)>(1-matg(0))) || (matg(1)<0)){
                matg.zeros();
                matrixxt.zeros();
            }
        }
    }
    else{
        if(S=='N'){
// alpha restrictions with no seasonality
            if((matg(0)>1+1/phivalue) || (matg(0)<1-1/phivalue)){
                matg.zeros();
                matrixxt.zeros();
            }
            if(T!='N'){
// beta restrictions with no seasonality
                if((matg(1)>(1+phivalue)*(2-matg(0))) || (matg(1)<matg(0)*(phivalue-1))){
                    matg.zeros();
                    matrixxt.zeros();
                }
            }
        }
        else{
            if(T=='N'){
// alpha restrictions with no trend
                if((matg(0)>2-matg(1)) ||  (matg(0)<(-2/(freq-1)))){
                    matg.zeros();
                    matrixxt.zeros();
                }
// gamma restrictions with no trend
                if((matg(1)>2-matg(0)) || (matg(1)<std::max(-freq*matg(0),0.0))){
                    matg.zeros();
                    matrixxt.zeros();
                }
            }
            else{
                double Bvalue = phivalue*(4-3*matg(2))+matg(2)*(1-phivalue) / freq;
                double Cvalue = sqrt(pow(Bvalue,2)-8*(pow(phivalue,2)*pow((1-matg(2)),2)+2*(phivalue-1)*(1-matg(2))-1)+8*pow(matg(2),2)*(1-phivalue) / freq);
                double Dvalue = (phivalue*(1-matg(0))+1)*(1-cos(theta))-matg(2)*((1+phivalue)*(1-cos(theta)-cos(freq*theta))+cos((freq-1)*theta)+phivalue*cos((freq+1)*theta))/(2*(1+cos(theta))*(1-cos(freq*theta)));
// alpha restriction
                if((matg(0)>((Bvalue + Cvalue)/(4*phivalue))) || (matg(0)<(1-1/phivalue-matg(2)*(1-freq+phivalue*(1+freq))/(2*phivalue*freq)))){
                    matg.zeros();
                    matrixxt.zeros();
                }
// beta restriction
                if((matg(1)>(Dvalue+matg(0)*(phivalue-1))) || (matg(1)<(phivalue-1)*(matg(2)/freq+matg(0)))){
                    matg.zeros();
                    matrixxt.zeros();
                }
// gamma restriction
                if((matg(2)>(1+1/phivalue-matg(0))) || (matg(2)<(std::max(1-1/phivalue-matg(0),0.0)))){
                    matg.zeros();
                    matrixxt.zeros();
                }
            }
        }
    }

    return optimizer(matrixxt,matrixF,matrixw,matyt,matg,hor,E,T,S,freq,tr,CFtype,normalize);
}