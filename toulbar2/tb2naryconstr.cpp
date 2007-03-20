
#include "tb2naryconstr.hpp"
#include "tb2wcsp.hpp"


NaryConstraint::NaryConstraint(WCSP *wcsp, EnumeratedVariable** scope_in, int arity_in, Cost defval)
			: AbstractNaryConstraint(wcsp, scope_in, arity_in), nonassigned(arity_in, &wcsp->getStore()->storeValue)
{
	default_cost = defval;
	store_top = default_cost < wcsp->getUb();
	xy = new BinaryConstraint(wcsp, &wcsp->getStore()->storeCost );
    propagate();
	pf = new TUPLES;
}

NaryConstraint::NaryConstraint(WCSP *wcsp)
			: AbstractNaryConstraint(wcsp), nonassigned(0, &wcsp->getStore()->storeValue)
{
	xy = NULL;
	pf = new TUPLES;
}


NaryConstraint::~NaryConstraint()
{
	if(pf) delete pf;
}



// USED ONLY DURING SEARCH to project the nary constraint 
// to the binary constraint xy, when all variables but 2 are assigned 
void NaryConstraint::projectNaryBinary()
{
	int indexs[2];
	EnumeratedVariable* unassigned[2];

	char* tbuf = new char [arity_ + 1]; 
	string t;

	int i,j = 0;
	for(i=0;i<arity_;i++) { 		
		if(getVar(i)->unassigned()) { 
			unassigned[j] = (EnumeratedVariable*) getVar(i); 
			indexs[j] = i;
			tbuf[i] = CHAR_FIRST;
			j++;
		} else tbuf[i] = getVar(i)->getValue() + CHAR_FIRST;
	}
	tbuf[arity_] =  '\0';
	t = tbuf;
	delete [] tbuf;
	
	EnumeratedVariable* x = unassigned[0];
	EnumeratedVariable* y = unassigned[1];
	xy->fillElimConstr(x,y);

	for (EnumeratedVariable::iterator iterx = x->begin(); iterx != x->end(); ++iterx) {
    for (EnumeratedVariable::iterator itery = y->begin(); itery != y->end(); ++itery) {	
		Value xval = *iterx;
		Value yval = *itery;
		t[indexs[0]] =  xval + CHAR_FIRST;			
		t[indexs[1]] =  yval + CHAR_FIRST;					
		xy->setcost(xval,yval,eval(t));
    }}
	BinaryConstraint* ctr = x->getConstr(y);   			

	/*if(ctr) {
		ctr->reconnect();
		ctr->addCosts(xy);
		ctr->propagate();
	} 
	else*/ 
	{
		xy->reconnect();
		xy->propagate();
	}	
}


// USED ONLY DURING SEARCH 
void NaryConstraint::assign(int varIndex) {
	assert(nonassigned > 2);
    if (connected(varIndex)) {
       deconnect(varIndex);	
	   nonassigned = nonassigned - 1;
	   if(nonassigned == 2) {
			deconnect();
			projectNaryBinary();
	   }
    }
}

#include <map>
using namespace std;


Cost NaryConstraint::eval( string s ) {
	TUPLES& f = *pf;
	TUPLES::iterator  it = f.find(s);
	if(it != f.end()) return it->second;
	else return default_cost;  
}

Cost NaryConstraint::evalsubstr( string& s, Constraint* ctr )
{
	int arity_in = s.size();
	int count = 0;
	char* cht = new char [arity_+1];
	cht[arity_] = '\0';
	
	for(int i=0;i<arity_in;i++) {
		int ind = getIndex( ctr->getVar(i) );
		if(ind >= 0) { cht[ind] = s[i]; count++; }	
	}
	Cost cost;
	if(count == arity_) cost = eval( string(cht) );
	else cost = 0;
	
	delete [] cht;
	return cost;
}    

void NaryConstraint::firstlex( string& t )
{
	int i;
	char* cht = new char [arity_+1];
	cht[arity_]='\0';

	for(i=0;i<arity_;i++) cht[i] = 0 + CHAR_FIRST;
	t = string(cht);
	delete [] cht;	
}

void NaryConstraint::first() 
{ 
	tuple_it = pf->begin();
}


bool NaryConstraint::nextlex( string& t, Cost& c)
{
	int i;
	bool finish = false;
	bool last = true;
	char* cht = new char [arity_+1];
	for(i=0;i<arity_;i++) cht[i] = t[i];
	cht[i]='\0';
	int pos = arity_-1;
	while(last && !finish) {
		last = (cht[pos] - CHAR_FIRST == getVar(pos)->getDomainSize() - 1);
		if(!last) cht[pos]++;
		else cht[pos] = 0 + CHAR_FIRST;
		pos--;  
		finish = (pos < 0) && last;
	}
	if(!finish) {
		t = string(cht);
		c = eval(t); 
	}
	delete [] cht;	
	return !finish;
}


bool NaryConstraint::next( string& t, Cost& c)
{
	if(tuple_it != pf->end()) { 
		t = tuple_it->first;
		c = tuple_it->second;
		tuple_it++;
		return true;
	}
	else return false;
}

void NaryConstraint::permute( EnumeratedVariable** scope_in )
{
	TUPLES* pf_old = pf;
	pf = new TUPLES;

	TUPLES::iterator it = pf_old->begin(); 
	while(it != pf_old->end()) {
		setTuple( it->first, it->second, scope_in );
		it++;
	}
	
	delete pf_old;
	for(int i=0; i<arity_; i++)
	{
		map<int,int>::iterator it_pos =  scope_inv.find(scope_in[i]->wcspIndex);
		int i_old = it_pos->second;
		it_pos->second = i;
				
		scope_inv[ scope_in[i]->wcspIndex ] = i;
		scope[i] = scope_in[i];

   	    DLink<ConstraintLink>* l = links[i];
   	    links[i] = links[i_old];
   	    links[i_old] = l;
   	    
   	    links[i]->content.scopeIndex = i;
   	    l->content.scopeIndex = i_old; 
	} 
}


// for adding a tuple in f
// scope_in contains the order of the values in string tin 
void NaryConstraint::setTuple( string tin, Cost c, EnumeratedVariable** scope_in )
{
	string t(tin);
	if(scope_in) {
		for(int i = 0; i < arity_; i++) {
			int pos = getIndex(scope_in[i]);
			t[pos] = tin[i];
		}  
	}
	(*pf)[t] = c;
}

void NaryConstraint::insertSum( string t1, Cost c1, Constraint* ctr1, string t2, Cost c2, Constraint* ctr2 )
{
	char* t = new char [arity_+1];

	for(int i = 0; i < arity_; i++) {
		EnumeratedVariable* v = scope[i]; 
		int pos = i;
		int pos1 = ctr1->getIndex(v);
		int pos2 = ctr2->getIndex(v);
		
		if((pos1 >= 0) && (pos2 >= 0)) {
			if(t1[pos1] != t2[pos2])  { delete [] t; return; }
			t[pos] = t1[pos1];
		}
		else if(pos1 >= 0) t[pos] = t1[pos1];			
		else if(pos2 >= 0) t[pos] = t2[pos2];			
	}  
	t[arity_] = '\0';
	(*pf)[string(t)] = c1 + c2;
	delete [] t;
}  


void NaryConstraint::sum( NaryConstraint* nary )
{
	deconnect();
	
	map<int,int> snew;
	set_union( scope_inv.begin(), scope_inv.end(),
		  	   nary->scope_inv.begin(), nary->scope_inv.end(),
		  	   inserter(snew, snew.begin()) );
	
	arity_ = snew.size();
	EnumeratedVariable** scope1 = scope;
	DLink<ConstraintLink>** links1 = links; 
	scope = new EnumeratedVariable* [arity_];
	links = new DLink<ConstraintLink>* [arity_];
		
	int i = 0;
	map<int,int>::iterator its = snew.begin();
	while(its != snew.end()) {
		EnumeratedVariable* var = (EnumeratedVariable*) wcsp->getVar(its->first);
		its->second = i;
		scope[i] =  var;
		int index1 = getIndex(var);
		if(index1 >= 0) {
			links[i] = links1[index1];
			ConstraintLink e = {this, i};
			links[i]->content = e;
		}
		else links[i] = nary->links[ nary->getIndex(var) ];

		i++;
		its++;
	}

	TUPLES& f1 = *pf;
	TUPLES& f2 = *nary->pf;
	TUPLES::iterator  it1 = f1.begin();
	TUPLES::iterator  it2 = f2.begin();
	TUPLES& f = * new TUPLES;
	pf = &f;

	string t1,t2;
	Cost c1,c2;   
	while(it1 != f1.end()) {
		t1 = it1->first;
		c1 =  it1->second;
		while(it2 != f2.end()) {
			t2 = it2->first;
			c2 =  it2->second;	
			insertSum(t1, c1, this, t2, c2, nary);
			it2++;
		}
		it1++;
	}
	
	scope_inv = snew;
	delete [] scope1;
	delete [] links1;
	
	reconnect();
}

// Projection of variable x of the nary constraint 
// complexity O(2|f|)
// this function is independent of the search 
void NaryConstraint::project( EnumeratedVariable* x )
{
	if(getIndex(x) < 0) return;
	assert(x->getDegree() == 1);

	
	string t,tnext,tproj;
	Cost c;

	TUPLES& f = *pf;	
	TUPLES fproj;
	
	
	int xindex = getIndex(x);
	
	// First part of the projection: complexity O(|f|) we swap positions between the projected variable and the last variable		
	TUPLES::iterator  it = f.begin();
	while(it != f.end()) {
		t = it->first;
		c =  it->second;
		
		string tswap(t);
		char a = tswap[arity_-1];
		tswap[arity_-1] = tswap[xindex];
		tswap[xindex] = a;
				
		fproj[tswap] = c + x->getCost(tswap[arity_-1] - CHAR_FIRST);		
		f.erase(t);		
				
		it++;
	} 
	// Second part of the projection: complexity O(|f|) as the projected variable is in the last position,
	// it is sufficient to look for tuples with the same arity-1 posotions. If there are less than d (domain of
	// the projected variable) tuples, we have also to perform the minimum with default_cost
	// this is only true when the tuples are LEXICOGRAPHICALY ordered	
    it = fproj.begin();
	if(it != fproj.end()) {
		t = it->first;
		c = it->second;
		bool end = false;
		unsigned int ntuples = 1;

		while(!end) {		
			it++;
			end = (it == fproj.end());
			bool sameprefix = false;
			
			Cost cnext = -1;
			if(!end) { 
				tnext = it->first;
				cnext = it->second;
				sameprefix = (t.compare(0,arity_-1,tnext,0,arity_-1) == 0);
				//cout << "<" << t << "," << c << ">   <" << tnext << "," << cnext << ">       : " << t.compare(0,arity_-1,tnext,0,arity_-1) << endl;
			}
			if(!sameprefix) {
				if(ntuples < x->getDomainInitSize()) 
					if(default_cost < c) c = default_cost;
				
				if(c != default_cost) f[ t.substr(0,arity_-1) ] = c;
				t = tnext;
				c = cnext;
				ntuples = 1;
			} else {
				ntuples++;
				if(cnext < c) c = cnext;
			}			
		}
	}
	
	// Update of internal structures: scope, indexes, links, arity_.
	x->deconnect(links[xindex]);
	DLink<ConstraintLink>* linkx = links[xindex];
	links[xindex] = links[arity_-1];
	links[arity_-1] = linkx;
	links[xindex]->content.scopeIndex = xindex;
	links[arity_-1]->content.scopeIndex = arity_-1;
	scope_inv.erase(scope[xindex]->wcspIndex);
	scope_inv[scope[arity_-1]->wcspIndex] = xindex;
	scope[ xindex ] = scope[arity_-1];
	scope[ arity_-1 ] = x;
	nonassigned = nonassigned - 1;
	arity_--;	
}



void NaryConstraint::print(ostream& os)
{
	TUPLES& f = *pf;
	os << endl << "f(";
	
	int unassigned_ = 0;
	 
	for(int i = 0; i < arity_;i++) {
		if(scope[i]->unassigned()) unassigned_++;
		os << scope[i]->wcspIndex;
		if(i < arity_-1) os << ",";
	}
	os << ")    ";
	os << " |f| = " << f.size();
	os << "    default_cost: " << default_cost;
	os << "       unassigned: " << (int) nonassigned << "   and real unassigned: " << unassigned_ << endl;
	os << "tuples: {";
	
	TUPLES::iterator  it = f.begin();
	while(it != f.end()) {
		string t = it->first;
		Cost c =  it->second;		
		it++;
		os << "<" << t << "," << c << ">";
		if(it != f.end()) os << " "; 
	} 
	os << "} " << endl;
}

void NaryConstraint::dump(ostream& os)
{
	TUPLES& f = *pf;
    os << arity_;
    for(int i = 0; i < arity_;i++) 
    {
        os << " " << scope[i]->wcspIndex;
    }
    os << default_cost << " " << f.size() << endl;
    
    TUPLES::iterator  it = f.begin();
    while(it != f.end()) {
        string t = it->first;
        Cost c =  it->second;       
        it++;
        os << t << " " << c << endl; 
    } 
}