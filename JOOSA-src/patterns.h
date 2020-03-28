/*
 * JOOS is Copyright (C) 1997 Laurie Hendren & Michael I. Schwartzbach
 *
 * Reproduction of all or part of this software is permitted for
 * educational or research use on condition that this copyright notice is
 * included in any copy. This software comes with no warranty of any
 * kind. In no event will the authors be liable for any damages resulting from
 * use of this software.
 *
 * email: hendren@cs.mcgill.ca, mis@brics.dk
 * 
 * Documentation follows this convention:
 * isntruction_to_replace_1
 * instruction_to_replace_2
 * ...
 * instruction_to_replace_N
 * ------>
 * instruction_to_replace_with_1
 * instruction_to_replace_with_2
 * ...
 * instruction_to_replace_with_M
 */

/* iload x        iload x        iload x
 * ldc 0          ldc 1          ldc 2
 * imul           imul           imul
 * ------>        ------>        ------>
 * ldc 0          iload x        iload x
 *                               dup
 *                               iadd
 */

int simplify_multiplication_right(CODE **c)
{ int x,k;
  if (is_iload(*c,&x) && 
      is_ldc_int(next(*c),&k) && 
      is_imul(next(next(*c)))) {
     if (k==0) return replace(c,3,makeCODEldc_int(0,NULL));
     else if (k==1) return replace(c,3,makeCODEiload(x,NULL));
     else if (k==2) return replace(c,3,makeCODEiload(x,
                                       makeCODEdup(
                                       makeCODEiadd(NULL))));
     return 0;
  }
  return 0;
}

/* ldc 0          ldc 1          ldc 2
 * iload x        iload x        iload x
 * imul           imul           imul
 * ------>        ------>        ------>
 * ldc 0          iload x        iload x
 *                               dup
 *                               iadd
 */

int simplify_multiplication_left(CODE **c)
{ int x,k;
  if (is_ldc_int(*c, &k) &&
      is_iload(next(*c), &x) &&
      is_imul(next(next(*c)))) 
    {
      if (k==0) return replace(c,3,makeCODEldc_int(0, NULL));
      else if (k==1) return replace(c,3,makeCODEiload(x, NULL));
      else if (k==2) return replace(c,3,makeCODEiload(x,
                                        makeCODEdup(
                                        makeCODEiadd(NULL))));
      return 0;
    }
  return 0;
}

/**
 * iload x        ldc y
 * ldc 1          ldc 1
 * idiv           idiv
 * -------->      -------->
 * iload x        ldc y
**/
int simplify_division_by_one_right(CODE **c)
{
  int x, y, k;
  if (is_iload(*c, &x) &&
      is_ldc_int(next(*c), &k) &&
      is_idiv(next(next(*c))))
      {
        if (k==1) return replace(c, 3, makeCODEiload(x, NULL));
        return 0;
      }
  else if (is_ldc_int(*c, &y) &&
           is_ldc_int(next(*c), &k) &&
           is_idiv(next(next(*c))))
           {
             if (k==1) return replace(c, 3, makeCODEldc_int(y, NULL));
             return 0;
           }
  return 0;
}


/* dup
 * astore x
 * pop
 * -------->
 * astore x
 */
int simplify_astore(CODE **c)
{ int x;
  if (is_dup(*c) &&
      is_astore(next(*c),&x) &&
      is_pop(next(next(*c)))) {
     return replace(c,3,makeCODEastore(x,NULL));
  }
  return 0;
}

/**
 * dup
 * istore x
 * pop
 * -------->
 * istore x
**/
int simplify_istore(CODE **c)
{
  int x;
  if (is_dup(*c) &&
      is_istore(next(*c), &x) &&
      is_pop(next(next(*c)))) {
        return replace(c, 3, makeCODEistore(x, NULL));
      }
  return 0;
}

/* goto L1
 * ...
 * L1:
 * goto L2
 * ...
 * L2:
 * --------->
 * goto L2
 * ...
 * L1:    (reference count reduced by 1)
 * goto L2
 * ...
 * L2:    (reference count increased by 1)  
 */
int simplify_goto_goto(CODE **c)
{ int l1,l2;
  if (is_goto(*c,&l1) && is_goto(next(destination(l1)),&l2) && l1>l2) {
     droplabel(l1);
     copylabel(l2);
     return replace(c,1,makeCODEgoto(l2,NULL));
  }
  return 0;
}

/**
 * if_cmp true_1  (if_cmp is some conditional instruction)
 * iconst_0
 * goto stop_2
 * true_1:
 * iconst_1
 * stop_2:
 * ifeq stop_0
 * ...
 * stop_0:
 * --------->
 * !if_cmp stop_0
 * ...
 * stop_0:
 */
int simplify_if_cmp(CODE **c)
{
  int true_l1, stop_l1, stop_l2, l4;
  int c1, c2;

  if (is_ifeq(*c, &true_l1) || is_ifne(*c, &true_l1) || is_ifnull(*c, &true_l1) || is_ifnonnull(*c, &true_l1) ||
    is_if_acmpeq(*c, &true_l1) || is_if_acmpne(*c, &true_l1) || is_if_icmpeq(*c, &true_l1) || is_if_icmpge(*c, &true_l1) || 
    is_if_icmpgt(*c, &true_l1) || is_if_icmple(*c, &true_l1) || is_if_icmplt(*c, &true_l1) || is_if_icmpne(*c, &true_l1)
  )
  {
    if (
        /* iconst_0
         * goto stop_2
         * ...
         * ifeq stop_0:
        */
        is_ldc_int(next(*c), &c1) && c1 == 0 && 
        is_goto(next(next(*c)), &stop_l1) && 
        is_ifeq(next(destination(stop_l1)), &stop_l2) &&

        /* > true_1:
         * iconst_1
         * stop_2:
        */
        is_ldc_int(next(destination(true_l1)), &c2) && c2 == 1 &&
        is_label(next(next(destination(true_l1))), &l4) && l4 == stop_l1)
        {
          if(!(uniquelabel(true_l1) && uniquelabel(stop_l1)))
          {
            return 0;
          }

          /* The labels are no longer necessary since we effectively skip them*/
          droplabel(true_l1);
          droplabel(stop_l1);
          
          if(is_ifeq(*c, &true_l1))
          {
            return replace(c, 7, makeCODEifne(stop_l2, NULL));
          }
          else if(is_ifne(*c, &true_l1))
          {
            return replace(c, 7, makeCODEifeq(stop_l2, NULL));
          }
          else if(is_if_acmpeq(*c, &true_l1))
          {
            return replace(c, 7, makeCODEif_acmpne(stop_l2, NULL));
          }
          else if(is_if_acmpne(*c,&true_l1))
          {
            return replace(c, 7, makeCODEif_acmpeq(stop_l2, NULL));
          }
          else if(is_if_icmpeq(*c, &true_l1))
          {
            return replace(c, 7, makeCODEif_icmpne(stop_l2, NULL));
          }
          else if(is_if_icmpge(*c, &true_l1))
          {
            return replace(c, 7, makeCODEif_icmplt(stop_l2, NULL));
          }
          else if(is_if_icmpgt(*c, &true_l1))
          {
            return replace(c, 7, makeCODEif_icmple(stop_l2, NULL));
          }
          else if(is_if_icmple(*c, &true_l1))
          {
            return replace(c, 7, makeCODEif_icmpgt(stop_l2, NULL));
          }
          else if(is_if_icmplt(*c, &true_l1))
          {
            return replace(c, 7, makeCODEif_icmpge(stop_l2, NULL));
          }
          else if(is_if_icmpne(*c, &true_l1))
          {
            return replace(c, 7, makeCODEif_icmpeq(stop_l2, NULL));
          }
          else if(is_ifnonnull(*c, &true_l1))
          {
            return replace(c, 7, makeCODEifnull(stop_l2, NULL));
          }
          else{
            return replace(c, 7, makeCODEifnonnull(stop_l2, NULL));
          }
        }
  }
  return 0;
}

/* iload x
 * ldc k   (0<=k<=127)
 * iadd
 * istore x
 * --------->
 * iinc x k
 */ 
int positive_increment(CODE **c)
{ int x,y,k;
  if (is_iload(*c,&x) &&
      is_ldc_int(next(*c),&k) &&
      is_iadd(next(next(*c))) &&
      is_istore(next(next(next(*c))),&y) &&
      x==y && 0<=k && k<=127) {
     return replace(c,4,makeCODEiinc(x,k,NULL));
  }
  return 0;
}

/* iload x
 * ldc k   (0<=k<=127)
 * isub
 * istore x
 * --------->
 * iinc x k
 */ 
int negative_increment(CODE **c) 
{
  int x,y,k;
  if (is_iload(*c,&x) &&
      is_ldc_int(next(*c),&k) &&
      is_isub(next(next(*c))) &&
      is_istore(next(next(next(*c))),&y) &&
      x==y && 0<=k && k<=127) {
     return replace(c,4,makeCODEiinc(x,-k,NULL));
  }
  return 0;
}

/**
 * iinc x 0
 * --------->
 * (empty)
 */ 
int remove_zero_increment(CODE **c)
{
  int x, amount;
  if (is_iinc(*c, &x, &amount) && amount==0) {
    return kill_line(c);
  }
  return 0;
}

/**
 * label: (empty)
 * --------->
 * (empty)
 */ 
int remove_dead_label(CODE **c)
{
  int label;
  if (is_label(*c, &label) && deadlabel(label)) {
    return kill_line(c);
  }
  return 0;
}

/**
 * nop
 * --------->
 * (empty)
 */
int remove_nop(CODE **c)
{
  if (is_nop(*c) && next(*c) != NULL)
  {
    return kill_line(c);
  }
  return 0;
}

/**
 *  Add patterns here so they can be used to optimize code
 */
void init_patterns(void) {
  /* Please try to keep these in alphabetical order, so it's easier to find their names during benchmarks*/
	ADD_PATTERN(negative_increment);
	ADD_PATTERN(positive_increment);
	ADD_PATTERN(simplify_astore);
	ADD_PATTERN(simplify_division_by_one_right);
	ADD_PATTERN(simplify_goto_goto);
	ADD_PATTERN(simplify_if_cmp);
	ADD_PATTERN(simplify_istore);
	ADD_PATTERN(simplify_multiplication_left);
	ADD_PATTERN(simplify_multiplication_right);
  ADD_PATTERN(remove_dead_label);
  ADD_PATTERN(remove_nop);
  ADD_PATTERN(remove_zero_increment);

}
