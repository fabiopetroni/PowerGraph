#ifndef _GL_NMF
#define _GL_NMF

#include "graphlab.hpp"
#include <graphlab/macros_def.hpp>

extern int D,M,N;
gl_types::core * glcore;
double * x1;
double * x2;

/*
function [w,h]=nmf(v,r,verbose)
%
% Jean-Philippe Brunet
% Cancer Genomics 
% The Broad Institute
% brunet@broad.mit.edu
%
% This software and its documentation are copyright 2004 by the
% Broad Institute/Massachusetts Institute of Technology. All rights are reserved.
% This software is supplied without any warranty or guaranteed support whatsoever. 
% Neither the Broad Institute nor MIT can not be responsible for its use, misuse, 
% or functionality. 
%
% NMF divergence update equations :
% Lee, D..D., and Seung, H.S., (2001), 'Algorithms for Non-negative Matrix 
% Factorization', Adv. Neural Info. Proc. Syst. 13, 556-562.
%
% v (n,m) : N (genes) x M (samples) original matrix 
%           Numerical data only. 
%           Must be non negative. 
%           Not all entries in a row can be 0. If so, add a small constant to the 
%           matrix, eg.v+0.01*min(min(v)),and restart.
%           
% r       : number of desired factors (rank of the factorization)
%
% verbose : prints iteration count and changes in connectivity matrix elements
%           unless verbose is 0 
%           
% Note : NMF iterations stop when connectivity matrix has not changed 
%        for 10*stopconv interations. This is experimental and can be
%        adjusted.
%
% w    : N x r NMF factor
% h    : r x M NMF factor


% test for negative values in v
if min(min(v)) < 0
error('matrix entries can not be negative');
return
end
if min(sum(v,2)) == 0
error('not all entries in a row can be zero');
return
end


[n,m]=size(v);
stopconv=40;      % stopping criterion (can be adjusted)
niter = 2000;     % maximum number of iterations (can be adjusted)

cons=zeros(m,m);
consold=cons;
inc=0;
j=0;

%
% initialize random w and h
%
w=rand(n,r);
h=rand(r,m); 


for i=1:niter

% divergence-reducing NMF iterations

x1=repmat(sum(w,1)',1,m);
h=h.*(w'*(v./(w*h)))./x1;
x2=repmat(sum(h,2)',n,1);
w=w.*((v./(w*h))*h')./x2;

% test convergence every 10 iterations

if(mod(i,10)==0)  
j=j+1;

% adjust small values to avoid undeflow
h=max(h,eps);w=max(w,eps);

% construct connectivity matrix
[y,index]=max(h,[],1);   %find largest factor
mat1=repmat(index,m,1);  % spread index down
mat2=repmat(index',1,m); % spread index right
cons=mat1==mat2;

if(sum(sum(cons~=consold))==0) % connectivity matrix has not changed
inc=inc+1;                     %accumulate count 
else
inc=0;                         % else restart count
end
if verbose                     % prints number of changing elements 
fprintf('\t%d\t%d\t%d\n',i,inc,sum(sum(cons~=consold))), 
end

if(inc>stopconv)
break,                % assume convergence is connectivity stops changing 
end 

consold=cons;

end
end
*/



void nmf_init(){
  x1 = new double[D];
  x2 = new double[D];
  for (int i=0; i<D; i++){
     x1[i] = x2[i] = 0;
  }
}
edge_list get_edges(gl_types::iscope & scope){
     return ((int)scope.vertex() < M) ? scope.out_edge_ids(): scope.in_edge_ids();
}
const vertex_data& get_neighbor(gl_types::iscope & scope, edge_id_t oedgeid){
     return ((int)scope.vertex() < M) ? scope.neighbor_vertex_data(scope.target(oedgeid)) :  scope.neighbor_vertex_data(scope.source(oedgeid));
}
     
void nmf_update_function(gl_types::iscope & scope, 
      gl_types::icallback & scheduler){


  /* GET current vertex data */
  vertex_data& user = scope.vertex_data();
 
  
  /* print statistics */
  if (debug&& (scope.vertex() == 0 || ((int)scope.vertex() == M-1) || ((int)scope.vertex() == M) || ((int)scope.vertex() == M+N-1))){
    printf("NMF: entering %s node  %u \n", (((int)scope.vertex() < M) ? "movie":"user"), (int)scope.vertex());   
    debug_print_vec((((int)scope.vertex() < M) ? "V " : "U") , user.pvec, D);
  }

  assert((int)scope.vertex() < M+N);

  user.rmse = 0;

  if (user.num_edges == 0){
    return; //if this user/movie have no ratings do nothing
  }

  double * buf = new double[(int)scope.vertex() < M ? M : N];
  double * ret = new double[D];
  for (int i=0; i<D; i++)
     ret[i] = 0;
  for (int i=0; i< (((int)scope.vertex() < M ) ? M: N); i++)
     buf[i] = 0;

  edge_list outs = get_edges(scope);
  timer t; t.start(); 
    
   foreach(graphlab::edge_id_t oedgeid, outs) {

      edge_data & edge = scope.edge_data(oedgeid);
      double v = edge.weight;
      const vertex_data  & movie = get_neighbor(scope, oedgeid);
      float prediction;     
      float sq_err = predict(user, movie, NULL, v , prediction);
      buf[scope.target(oedgeid) - M] = v/prediction;
      user.rmse += sq_err;
    }

    foreach(graphlab::edge_id_t oedgeid, outs){
      
      vertex_data  & movie = scope.neighbor_vertex_data(scope.target(oedgeid)); 
       for (int j=0; j<D; j++){
         ret[j] += movie.pvec[j] * buf[scope.target(oedgeid) - M];
       }
    }
     

  counter[EDGE_TRAVERSAL] += t.current_time();

  double * px;
  if ((int)scope.vertex() < M)
     px = x1;
  else 
     px = x2;

  for (int i=0; i<D; i++){
     assert(x1[i] != 0);
     user.pvec[i] *= ret[i] / x1[i];
  }

  delete[] buf;
}

void pre_user_iter(){
  for (int i=0; i<M; i++){
    const vertex_data * data = &glcore->graph().vertex_data(i);
    for (int i=0; i<D; i++){
      x1[i] += data->pvec[i];
    }
  }
}
void pre_movie_iter(){
  for (int i=M; i<M+N; i++){
    const vertex_data * data = &glcore->graph().vertex_data(i);
    for (int i=0; i<D; i++){
      x2[i] += data->pvec[i];
    }
  }
}

void nmf(gl_types::core * _glcore){

   glcore = _glcore;
  
   nmf_init();
   std::vector<vertex_id_t> rows,cols;
   for (int i=0; i< M; i++)
      rows.push_back(i);
   for (int i=M; i< M+N; i++)
      cols.push_back(i);
 

   for (int j=1; j<= svd_iter+1; j++){
	  pre_user_iter();
      
     glcore->add_tasks(rows, nmf_update_function, 1);
     glcore->start();

     pre_movie_iter();
     glcore->add_tasks(cols, nmf_update_function, 1);
     glcore->start();

   }
}

#include "graphlab/macros_undef.hpp"
#endif
