#include "pch.h"
#include "LayerStack.h"

PlagC::LayerStack::LayerStack()
{
	
}

PlagC::LayerStack::~LayerStack()
{
	for (Layer* it : m_Layers)
		delete it;
}

void PlagC::LayerStack::PushLayer(Layer* layer)
{
	m_Layers.emplace(m_Layers.begin() + m_LayerInsert, layer);
	m_LayerInsert++;
	layer->OnAttach();
}

void PlagC::LayerStack::PopLayer(Layer* layer)
{
	auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
	if (it != m_Layers.end())
	{
		m_Layers.erase(it);
		m_LayerInsert--;
	}
}

void PlagC::LayerStack::PushOverlay(Layer* overlay)
{
	m_Layers.emplace_back(overlay);
	overlay->OnAttach();
}

void PlagC::LayerStack::PopOverlay(Layer* overlay)
{
	auto it = std::find(m_Layers.begin(), m_Layers.end(), overlay);
	if (it != m_Layers.end())
		m_Layers.erase(it);
}
