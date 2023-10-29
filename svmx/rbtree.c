#include "pch.h"
#include "rbtree.h"
#include "rbtree_augmented.h"

#pragma warning(push)
#pragma warning(disable:4706)
struct rb_node* rb_prev(const struct rb_node* node) {
	struct rb_node* parent;

	if (RB_EMPTY_NODE(node))
		return NULL;

	/*
	* If we have a left-hand child, go down and then right as far
	* as we can.
	*/
	if (node->rb_left) {
		node = node->rb_left;
		while (node->rb_right)
			node = node->rb_right;
		return (struct rb_node*)node;
	}

	/*
	* No left-hand children. Go up till we find an ancestor which
	* is a right-hand child of its parent.
	*/
	while ((parent = rb_parent(node)) && node == parent->rb_left)
		node = parent;

	return parent;
}
#pragma warning(pop)

struct rb_node* rb_last(const struct rb_root* root) {
	struct rb_node* n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}

#pragma warning(push)
#pragma warning(disable:4706)
struct rb_node* rb_next(const struct rb_node* node) {
	struct rb_node* parent;

	if (RB_EMPTY_NODE(node))
		return NULL;

	/*
	* If we have a right-hand child, go down and then left as far
	* as we can.
	*/
	if (node->rb_right) {
		node = node->rb_right;
		while (node->rb_left)
			node = node->rb_left;
		return (struct rb_node*)node;
	}

	/*
	* No right-hand children. Everything down and left is smaller than us,
	* so any 'next' node must be in the general direction of our parent.
	* Go up the tree; any time the ancestor is a right-hand child of its
	* parent, keep going up. First time it's a left-hand child of its
	* parent, said parent is our 'next' node.
	*/
	while ((parent = rb_parent(node)) && node == parent->rb_right)
		node = parent;

	return parent;
}
#pragma warning(pop)

static inline void dummy_propagate(struct rb_node* node, struct rb_node* stop)
{
	UNREFERENCED_PARAMETER(node);
	UNREFERENCED_PARAMETER(stop);
}

static inline void dummy_copy(struct rb_node* old, struct rb_node* new) {
	UNREFERENCED_PARAMETER(old);
	UNREFERENCED_PARAMETER(new);
}

// ��չ��ת����
static inline void dummy_rotate(struct rb_node* old, struct rb_node* new) {
	UNREFERENCED_PARAMETER(old);
	UNREFERENCED_PARAMETER(new);
}

static const struct rb_augment_callbacks dummy_callbacks = {
	.propagate = dummy_propagate,
	.copy = dummy_copy,
	.rotate = dummy_rotate
};

/*
* Helper function for rotations:
* - old's parent and color get assigned to new
* - old gets assigned new as a parent and 'color' as a color.
*/
static inline void
__rb_rotate_set_parents(struct rb_node* old, struct rb_node* new,
	struct rb_root* root, int color) {
	// ��ȡold�ĸ��ڵ�
	struct rb_node* parent = rb_parent(old);

	new->__rb_parent_color = old->__rb_parent_color;
	rb_set_parent_color(old, new, color);
	// �Ѹ��ڵ�ĺ��ӽ��old�滻��new
	__rb_change_child(old, new, parent, root);
}

static inline void rb_set_black(struct rb_node* rb) {
	rb->__rb_parent_color += RB_BLACK;
}

/* @parent: ��ƽ���㣬 root�Ǻ�����ĸ��ڵ� */
static inline void
____rb_erase_color(struct rb_node* parent, struct rb_root* root,
	void (*augment_rotate)(struct rb_node* old, struct rb_node* new)) {
	NT_ASSERT(parent != NULL);
	struct rb_node* node = NULL, * sibling, * tmp1, * tmp2;


	while (TRUE)
	{
		/*
		 * Loop invariants:��ѭ��������
		 * - node is black (or NULL on first iteration)
		 * - node is not the root (parent is not NULL)
		 * - All leaf paths going through parent and node have a
		 *   black node count that is 1 lower than other leaf paths.
		 *   �����о���parent��node����·���еĺ�ɫ�������������·������1����
		 */
		sibling = parent->rb_right;
		if (node != sibling) { /* node == parent->rb_left */
			// ɾ�����ӽ�㽫���²�ƽ��
			if (rb_is_red(sibling)) {// �Һ���Ϊ��ɫ
				/*
				* Case 1 - left rotate at parent
				*
				*     P               S
				*    / \             / \
				*   N   s    -->    p   Sr
				*      / \         / \
				*     Sl  Sr      N   Sl
				*/
				/*
				* ��ΪsΪ��ɫ����������4��Sl��Sr��Ϊ��ɫ
				* ������������
				*/
				tmp1 = sibling->rb_left;
				parent->rb_right = tmp1;
				sibling->rb_left = parent;
				// ��Sl�ĸ��ڵ�����Ϊp,Sl����ɫΪ��ɫ
				rb_set_parent_color(tmp1, parent, RB_BLACK);
				// �ڵ�p�ɺ�ת�죬���s�ɺ�ת��
				__rb_rotate_set_parents(parent, sibling, root,
					RB_RED);

				augment_rotate(parent, sibling);
				sibling = tmp1;
			}
			/*
			* ������������if�������ƽ������Ϊ��ɫ
			* SlΪ��ɫ
			*
			* ���򣬴�ƽ������Һ���(S)�����Ϊ��ɫ����ʱ��ƽ����
			* ����ɫδ֪
			*/
			tmp1 = sibling->rb_right;
			if (!tmp1 || rb_is_black(tmp1)) {
				// ���´���SrΪ�ջ��ߺ�ɫ�����
				tmp2 = sibling->rb_left;
				// ���Sl�ǿջ��ߺ�ɫ
				if (!tmp2 || rb_is_black(tmp2)) {
					/*
					 * Case 2 - sibling color flip
					 * (p could be either color here)
					 *
					 *    (p)           (p)
					 *    / \           / \
					 *   N   S    -->  N   s
					 *      / \           / \
					 *     Sl  Sr        Sl  Sr
					 *
					 * This leaves us violating 5) which
					 * can be fixed by flipping p to black
					 * if it was red, or by recursing at p.
					 * p is red when coming from Case 1.
					 */
					 // S -> s
					rb_set_parent_color(sibling, parent,
						RB_RED);
					if (rb_is_red(parent)) {
						/*
						*		 p			 P
						*		/ \			/ \
						*	   N   s  -->  N   s
						*	      / \         / \
						*        Sl  Sr      Sl  Sr
						*/
						// Υ������4������ p -> P, �ﵽƽ��
						rb_set_black(parent);
					}
					else {
						/*
						* �����ƽ�����Ѿ��Ǻ�ɫ�����ܼ򵥵Ľ�
						* ����ɫ����Ϊ��ɫ��
						* ��������丸�ڵ����ɫһ����Υ������4
						*/
						// ����ƽ��������Ϊnode
						node = parent;
						// ȡ���丸���
						parent = rb_parent(node);
						/*
						* ������ڸ���������غ������е���
						*/
						if (parent)
							continue;
						// �����ƽ�����Ѿ��������ˣ�����Ҫ����
					}
					break;
				}

				/*
				* ���±���sl������Ϊ��ɫ
				* SrΪ�ջ��ߺ�ɫ
				* ������4��֪,SΪ��ɫ
				*/
				/*
				 * Case 3 - right rotate at sibling
				 * (p could be either color here)
				 *
				 *   (p)           (p)
				 *   / \           / \
				 *  N   S    -->  N   sl
				 *     / \             \
				 *    sl  Sr            S
				 *                       \
				 *                        Sr
				 *
				 * Note: p might be red, and then both
				 * p and sl are red after rotation(which
				 * breaks property 4). This is fixed in
				 * Case 4 (in __rb_rotate_set_parents()
				 *         which set sl the color of p
				 *         and set p RB_BLACK)
				 *
				 *   (p)            (sl)
				 *   / \            /  \
				 *  N   sl   -->   P    S
				 *       \        /      \
				 *        S      N        Sr
				 *         \
				 *          Sr
				 */

				 /*
				 *   (p)           (p)
				 *   / \           / \
				 *  N   S    -->  N   sl
				 *     / \             \
				 *    sl  Sr            S
				 *      \			   / \
				 *       X            X  Sr
				 *
				 */
				tmp1 = tmp2->rb_right;
				sibling->rb_left = tmp1;
				tmp2->rb_right = sibling;
				parent->rb_right = tmp2;

				if (tmp1)
					rb_set_parent_color(tmp1, sibling,
						RB_BLACK);

				augment_rotate(sibling, tmp2);
				tmp1 = sibling;
				sibling = tmp2;
			}
			/*
			 * Case 4 - left rotate at parent + color flips
			 * (p and sl could be either color here.
			 *  After rotation, p becomes black, s acquires
			 *  p's color, and sl keeps its color)
			 *
			 *      (p)             (s)
			 *      / \             / \
			 *     N   S     -->   P   Sr
			 *        / \         / \
			 *      (sl) sr      N  (sl)
			 */

			tmp2 = sibling->rb_left;
			parent->rb_right = tmp2;
			sibling->rb_left = parent;
			// ����SrΪ��ɫ
			rb_set_parent_color(tmp1, sibling, RB_BLACK);
			if (tmp2)
				rb_set_parent(tmp2, parent);


			__rb_rotate_set_parents(parent, sibling, root,
				RB_BLACK);

			augment_rotate(parent, sibling);
			break;
		}
		else {
			sibling = parent->rb_left;
			// �ֵܽ��Ϊ��ɫ
			if (rb_is_red(sibling)) {
				/* Case 1 - right rotate at parent */
				/*
				*		P            S
				*      / \          / \
				*	  s   N   ->  SL   p
				*    / \              / \
				*   SL SR            SR  N
				*/
				tmp1 = sibling->rb_right;
				parent->rb_left = tmp1;
				sibling->rb_right = parent;
				// ����tmp1����ɫ�͸��ڵ�
				rb_set_parent_color(tmp1, parent, RB_BLACK);
				__rb_rotate_set_parents(parent, sibling, root,
					RB_RED);

				augment_rotate(parent, sibling);
				sibling = tmp1;
			}
			/* ��ȡ�ֵܽڵ������ */
			tmp1 = sibling->rb_left;
			// �ֵܽڵ�û�����ӻ��������Ǻ�ɫ
			if (!tmp1 || rb_is_black(tmp1)) {
				// �ֵܽڵ���Һ���
				tmp2 = sibling->rb_right;
				// �Һ��Ӳ����ڻ����Һ����Ǻ�ɫ
				if (!tmp2 || rb_is_black(tmp2)) {
					/* Case 2 - sibling color flip */
					// �����ֵܽڵ�Ϊ��ɫ
					rb_set_parent_color(sibling, parent,
						RB_RED);
					NT_ASSERT(parent != NULL);
					if (rb_is_red(parent))
						rb_set_black(parent);
					else {
						node = parent;
						parent = rb_parent(node);
						if (parent)
							continue;
					}
					break;
				}
				/* Case 3 - left rotate at sibling */
				/*
				*		P            P
				*	   / \          / \
				*	  S   N   ->   sr  N
				*      \          /
				*       sr       S
				*		/		  \
				*	   X		   X
				*/
				tmp1 = tmp2->rb_left;
				sibling->rb_right = tmp1;
				tmp2->rb_left = sibling;
				parent->rb_left = tmp2;
				if (tmp1)
					rb_set_parent_color(tmp1, sibling,
						RB_BLACK);
				augment_rotate(sibling, tmp2);
				tmp1 = sibling;
				sibling = tmp2;
			}
			/* Case 4 - right rotate at parent + color flips */
			/*
			*			P             S
			*		   / \           / \
			*         S   N     ->  SL  P
			*		 / \               / \
			*       sl  sr            sr  N
			*/
			tmp2 = sibling->rb_right;
			parent->rb_left = tmp2;
			sibling->rb_right = parent;
			rb_set_parent_color(tmp1, sibling, RB_BLACK);
			if (tmp2)
				rb_set_parent(tmp2, parent);
			__rb_rotate_set_parents(parent, sibling, root,
				RB_BLACK);
			augment_rotate(parent, sibling);
			break;
		}
	}
}

// ɾ����������
void rb_erase(struct rb_node* node, struct rb_root* root) {
	struct rb_node* rebalance;
	// ������������ɾ��
	rebalance = __rb_erase_augmented(node, root, &dummy_callbacks);
	if (rebalance) // �ָ������������
		____rb_erase_color(rebalance, root, dummy_rotate);
}

void rb_replace_node(struct rb_node* victim, struct rb_node* new,
	struct rb_root* root) {
	struct rb_node* parent = rb_parent(victim);

	/* Copy the pointers/colour from the victim to the replacement */
	*new = *victim;

	/* Set the surrounding nodes to point to the replacement */
	if (victim->rb_left)
		rb_set_parent(victim->rb_left, new);
	if (victim->rb_right)
		rb_set_parent(victim->rb_right, new);
	__rb_change_child(victim, new, parent, root);
}

// ���ڵ㸸�ڵ㣬��Ϊ��ɫ��ʾ��ɫ��λ��0.
static inline struct rb_node* rb_red_parent(struct rb_node* red) {
	return (struct rb_node*)red->__rb_parent_color;
}

/*
* ����ڵ����ת����ɫ�������ڲ�����
*/
static inline void
__rb_insert(struct rb_node* node, struct rb_root* root,
	void (*augment_rotate)(struct rb_node* old, struct rb_node* new)) {
	
	struct rb_node* parent = rb_red_parent(node), * gparent, * tmp;

	while (TRUE)
	{
		/*
		* Loop invariant: node is red.(node�Ǻ�ɫ�ڵ㣩
		*/
		if (!parent) {
			/*
			* The inserted node is root. Either this is the
			* first node, or we recursed at Case 1 below and
			* are no longer violating 4).
			*/
			rb_set_parent_color(node, NULL, RB_BLACK);
			break;
		}

		// ���ڵ��Ǻ�ɫ������һ����ɫ��㣬�����ƻ�ƽ��
		/*
		* If there is a black parent, we are done.
		* Otherwise, take some corrective action as,
		* per 4), we don't want a red root or two 
		* consecutive red nodes.
		*/
		if (rb_is_black(parent))
			break;

		// parent����һ���Ǻ�ɫ�ڵ�,
		// ������4��֪,gparentһ���Ǻ�ɫ
		gparent = rb_red_parent(parent);

		tmp = gparent->rb_right;
		if (parent != tmp) {
			if (tmp && rb_is_red(tmp)) {
				/*
				* Case 1 - node's uncle is red (color flips).
				*
				*		G			g
				*      / \         / \
				*     p   u  -->  P   U
				*    /           /
				*   n           n
				*
				* However, since g's parent might be red, and
				* 4) does not allow this, we need to recurse
				* at g.
				*/
				rb_set_parent_color(tmp, gparent, RB_BLACK);
				rb_set_parent_color(parent, gparent, RB_BLACK);

				// �ݹ�������һ�ִ���
				node = gparent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			tmp = parent->rb_right;
			if (node == tmp) {
				/*
				* Case 2 - node's uncle is black and node is
				* the parent's right child (left rotate at parent).
				*		G			G
				*	   / \         / \
				*     p   U       n   U
				*      \         / 
				*       n       p
				* This still leaves us in violation 4), the
				* continuation into Case 3 will fix that.
				*/
				tmp = node->rb_left;
				parent->rb_right = tmp;
				node->rb_left = parent;
				if (tmp)
					rb_set_parent_color(tmp, parent,
						RB_BLACK);
				rb_set_parent_color(parent, node, RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_right;
			}

			/*
			* Case 3 - node's uncle is black and node is
			* the parent's left child (right rotate at gparent)
			* 
			*		G			P
			*      / \         / \
			*     p   U  -->  n   g
			*    /                 \
			*   n                   U
			*/
			gparent->rb_left = tmp;
			parent->rb_right = gparent;
			if (tmp)
				rb_set_parent_color(tmp, gparent, RB_BLACK);
			__rb_rotate_set_parents(gparent, parent, root, RB_RED);
			augment_rotate(gparent, parent);
			break;
		}
		else {
			tmp = gparent->rb_left;
			if (tmp && rb_is_red(tmp)) {
				/* Case 1 - color flips */
				rb_set_parent_color(tmp, gparent, RB_BLACK);
				rb_set_parent_color(parent, gparent, RB_BLACK);
				node = gparent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			tmp = parent->rb_left;
			if (node == tmp) {
				/* Case 2 - right rotate at parent */
				tmp = node->rb_right;
				parent->rb_left = tmp;
				node->rb_right= parent;
				if (tmp)
					rb_set_parent_color(tmp, parent,
						RB_BLACK);
				rb_set_parent_color(parent, node, RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_left;
			}

			/* Case 3 - left rotate at gparent */
			gparent->rb_right = tmp; /* == parent->rb_left */
			parent->rb_left = gparent;
			if (tmp)
				rb_set_parent_color(tmp, gparent, RB_BLACK);
			__rb_rotate_set_parents(gparent, parent, root, RB_RED);
			augment_rotate(gparent, parent);
			break;
		}
	}
}

// ���ڲ���Ľڵ������ת����ɫ����
void rb_insert_color(struct rb_node* node, struct rb_root* root) {
	// Լ���²���Ľڵ��Ǻ�ɫ�ģ����ٴ�������
	__rb_insert(node, root, dummy_rotate);
}