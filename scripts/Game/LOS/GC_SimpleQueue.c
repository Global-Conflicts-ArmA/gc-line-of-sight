class GC_SimpleQueue<Class T>
{
	protected ref GC_QueueElement<T> m_Start;
	protected GC_QueueElement<T> m_End;
	protected int m_iCounter = 0;

	void Enqueue(T item)
	{
		GC_QueueElement<T> element = new GC_QueueElement<T>(item);
		if (m_Start)
		{
			m_End.m_Next = element;
			m_End = element;
		}
		else
		{
			m_Start = element;
			m_End = element;
		}
		m_iCounter++;
	}

	T Deque()
	{
		T item;

		if (m_Start)
		{
			item = m_Start.m_Item;
			m_Start = m_Start.m_Next;
			m_iCounter--;
			
			if (!m_Start)
				m_End = null;
		}
	
		return item;
	}

	bool IsEmpty()
	{
		return !m_Start;
	}

	void Clear()
	{
		while (m_Start)
			m_Start = m_Start.m_Next;
		
		m_End = null;
		m_iCounter = 0;
	}
	
	int Count()
	{
		return m_iCounter;
	}
}

class GC_QueueElement<Class T>
{
	ref GC_QueueElement<T> m_Next;
	T m_Item;

	void GC_QueueElement(T item)
	{
		m_Item = item;
	}
}