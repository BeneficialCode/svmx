#pragma once

/*
* ��ǿ�ͺ������һ����ÿ�������洢�ˡ�һЩ���������ݵĺ����
*/
struct rb_augment_callbacks {
	void (*propagate)(struct rb_node* node, struct rb_node* stop);
	void (*copy)(struct rb_node* old, struct rb_node* new);
	void (*rotate)(struct rb_node* old, struct rb_node* new);
};

#define RB_RED		0
#define RB_BLACK	1

// ���˫�׽��ĵ�ַ
#define __rb_parent(pc)		((struct rb_node*)(pc & ~3))

// �����ɫ����
#define __rb_color(pc)		((pc) & 1)
#define __rb_is_black(pc)	__rb_color(pc)
#define __rb_is_red(pc)		(!__rb_color(pc))
#define rb_color(rb)		__rb_color((rb)->__rb_parent_color)
// �ж���ɫ�����Ƿ�Ϊ��ɫ
#define rb_is_red(rb)		__rb_is_red((rb)->__rb_parent_color)
// �ж���ɫ�����Ƿ�Ϊ��ɫ
#define rb_is_black(rb)		__rb_is_black((rb)->__rb_parent_color)

// ���ý���˫�׽����׵�ַ����ɫ
static inline void rb_set_parent(struct rb_node* rb, struct rb_node* p)
{
	rb->__rb_parent_color = rb_color(rb) + (ULONG_PTR)p;
}

static inline void rb_set_parent_color(struct rb_node* rb,
	struct rb_node* p, int color) {
	rb->__rb_parent_color = (ULONG_PTR)p + color;
}

static inline void
__rb_change_child(struct rb_node* old, struct rb_node* new,
	struct rb_node* parent, struct rb_root* root) {
	if (parent) {
		if (parent->rb_left == old)
			parent->rb_left = new;
		else
			parent->rb_right = new;
	}
	else {
		root->rb_node = new;
	}
}

static struct rb_node*
__rb_erase_augmented(struct rb_node* node, struct rb_root* root,
	const struct rb_augment_callbacks* augment) {
	// ��ɾ������������
	struct rb_node* child = node->rb_right;
	// ��ɾ������������
	struct rb_node* tmp = node->rb_left;
	struct rb_node* parent = NULL, * rebalance = NULL;
	ULONG_PTR pc;

	if (!tmp) { /* ��ɾ������������� */
		/*
		* Case 1: node to earse has no more than 1 child (easy!)
		* 
		* Note that if there is one child it must be red due to 5)
		* and node must be black due to 4). We adjust colors locally
		* so as to bypass __rb_erase_color() later on.
		*/
		pc = node->__rb_parent_color;
		parent = __rb_parent(pc);
		/*
		* ��node����������Ϊnode���ڵ�����������node��λ��
		* ���������������Բ���
		*/ 
		__rb_change_child(node, child, parent, root);
		if (child) {
			/*
			* ��ɾ����������������������5�Ƴ���������Ϊ��ɫ���
			* ������4����һ���Ƴ���ɾ��������ɫΪ��ɫ
			* ������������ɫ����Ϊnode������ɫ������ɫ
			*/
			child->__rb_parent_color = pc;
			/*
			* ��ʱ���������Ҫ�ٽ���ƽ�����
			*/ 
			rebalance = NULL;
		}
		else {
			/*
			* ��ɾ���������������
			* 1.�����ɾ�����Ϊ��ɫ����ôɾ������ܻ᲻��������5��
			* ������Ҫ��ƽ��
			* 2.�����ɾ�����Ϊ��ɫ����ôɾ���󲻻�Υ������5��
			* ����Ҫ����ƽ�����
			*/ 
			rebalance = __rb_is_black(pc) ? parent : NULL;
		}
		tmp = parent;
	}
	else if (!child) { // ��ɾ��������������
		/*
		* Still case 1, but this time the child is node->rb_left 
		* ������5���Ƴ���������Ϊ��ɫ���
		* ������4��֪����ɾ�������ɫΪ��ɫ
		*/
		// ����������ɫ����Ϊ��ɫ
		tmp->__rb_parent_color = pc = node->__rb_parent_color;
		// ɾ�����
		parent = __rb_parent(pc);
		__rb_change_child(node, tmp, parent, root);
		// ɾ��������Ҫ����ƽ�����
		rebalance = NULL;
		tmp = parent;
	}
	else {
		/*
		* ��ɾ����������������,��Ҫȷ����ɾ������ֱ�Ӻ�� 
		* �ҵ������������ɾ����㣬Ȼ����ɾ��ֱ�Ӻ��
		* 
		*/
		struct rb_node* successor = child, * child2 = NULL;

		tmp = child->rb_left;
		if (!tmp) { // ��̽��û��������
			/*
			* Case 2: node's successor is its right child
			* 
			* 
			*     (n)		   (s)
			*	 /   \	      /   \
			*  (x)   (y) -> (x)	  (y)
			*		/             /
			*     (p)           (p)
			*     /             /
			*   (s)	          (c)
			*	  \
			*     (c)
			*/
			
			// �˴�ʹ��parent��¼��ֱ�Ӻ�̣���δʵ��ִ���滻����
			parent = successor;
			// child2Ϊֱ�Ӻ�̵�������
			child2 = successor->rb_right;

			augment->copy(node, successor);
		}
		else {// ��̽����������ǿ�
			/*
			 * Case 3: node's successor is leftmost under
			 * node's right child subtree
			 *
			 *    (n)          (s)
			 *    / \          / \
			 *  (x) (y)  ->  (x) (y)
			 *      /            /
			 *    (p)          (p)
			 *    /            /
			 *  (s)          (c)
			 *    \
			 *    (c)
			 */
			/*
			* �ҵ���ɾ������������������µĽ��
			*/
			do
			{
				parent = successor;
				successor = tmp;
				tmp = tmp->rb_left;
			} while (tmp);
			// child2Ϊֱ�Ӻ�̵�������
			child2 = successor->rb_right;

			parent->rb_left = child2;

			successor->rb_right = child;

			rb_set_parent(child, successor);

			augment->copy(node, successor);
			augment->propagate(parent, successor);
		}

		/* ��N����������ֲ��S��� */
		tmp = node->rb_left;
		successor->rb_left = tmp;
		rb_set_parent(tmp, successor);

		/* N�ĸ��ڵ���S������ϵ */
		pc = node->__rb_parent_color;
		tmp = __rb_parent(pc);
		__rb_change_child(node, successor, tmp, root);

		if (child2) { // ֱ�Ӻ����������
			rb_set_parent_color(child2, parent, RB_BLACK);
			rebalance = NULL;
		}
		else {
			/*
			* ��ֱ�Ӻ��Ϊ��ɫ��㣬ɾ��һ����ɫ��㽫Υ������5
			* ��Ҫ���к������ƽ�����
			*/
			rebalance = rb_is_black(successor) ? parent : NULL;
		}
		successor->__rb_parent_color = pc;
		tmp = successor;
	}

	augment->propagate(tmp, NULL);
	return rebalance;
}